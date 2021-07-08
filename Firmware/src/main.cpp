#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoJson.h>

int reconnectAttempt = 0;
int WIFI_MAX_RECONNECT_ATTEMPTS = 3;
bool USE_WIFI = true;
bool EMERGENCY_CALL = false;

String BoardID = "defaultBoardID";
bool isMaster;

char logPath[100];
char doorStatusPath[100];
char jsonStatusPath[100];

// Your GPRS credentials, if any
const char GPRS_APN[] = "";
const char GPRS_User[] = "";
const char GPRS_Pass[] = "";

//FREERTOS
#define configMAX_PRIORITIES 10
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 10
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 100000
#define INCLUDE_vTaskDelay  1
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
	#include "freertos/task.h"
}

//ESPAsyncWiFiManager 
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>        
Preferences preferences;
AsyncWebServer server(80);
DNSServer dns;
#define ACCESS_POINT_NAME "0-Key Access Point"

//AsyncMqtt
#include <AsyncMqttClient.h>
#define MQTT_HOST "mqtt.gattodubbioso.tk"
#define MQTT_PORT 1883
AsyncMqttClient mqttClient;

//TIMERS
TimerHandle_t emergencyCallTimer;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
TimerHandle_t incomingCallTimer;
TimerHandle_t piezoTimer;
TimerHandle_t doorTimer;
TimerHandle_t doorwayOutTimer;
TimerHandle_t doorwayListenTimer;
TimerHandle_t RFTimer;
TimerHandle_t gyroTimer;
TimerHandle_t simTimer;

//SIM800L

#define DEFAULT_ADMIN_PHONE "+393333333333"
#define DEFAULT_CUSTOMER_PHONE "+393333333334"
//#define DUMP_AT_COMMANDS                //Uncomment to debug AT commands
const char simPIN[]   = "";               // SIM card PIN code, if any

String admin_number, customer_number, master_number, slave_number;

#define SIM800L_IP5306_VERSION_20200811
#define TINY_GSM_DEBUG          SerialMon
#include "sim_utilities.h"
#define SerialMon Serial                 // Set debug serial connection
#define SerialAT  Serial1                // Set serial connection for AT commands (to the module)
#define TINY_GSM_MODEM_SIM800            // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024     // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
#define uS_TO_S_FACTOR 1000000ULL       //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60               //Time ESP32 will go to sleep (in seconds)

TinyGsmClient client(modem);
#include "sim_functions.h"

//MPU6050 Acc/Gyro - I2C
int16_t GYRO_THRESHOLD = 1000;
#define I2C_SDA_2            18
#define I2C_SCL_2            19
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
//int16_t ax, ay, az;
int16_t gx, gy, gz;
MPU6050 accelgyro(0x68); // <-- use for AD0 high
#include "gyro_functions.h"

//SERVO
#include <ESP32Servo.h>
#define SERVO_PIN 25
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
int SERVO_OPEN = 0;
int SERVO_CLOSED = 180;
Servo doorServo;  // create servo object to control a servo

//DOOR Functions
int timeListenPiezo;
int timeListenDoorway;
int timeOpenDoor;
int timeOpenDoorway;
#define DOORWAY_THRESHOLD 512
#define DOORWAY_IN_PIN 15
#define DOORWAY_OUT_PIN 14
#define PIEZO_PIN 34
int PIEZO_THRESHOLD = 1200;
bool DOORWAY_LISTENING = false;
bool PIEZO_LISTENING = false;
#include "door_functions.h"

//RF FS1000A Receiver
String RF_password = "password";
#define RF_RX_PIN 35
#define RF_TX_PIN 99
#define RH_ASK_MAX_MESSAGE_LEN 16
bool RF_enable;
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile
RH_ASK driver(2000, RF_RX_PIN, RF_TX_PIN, 0);
#include "rf_functions.h"
#include "mqtt_functions.h"

void connectToMqtt() {
  SerialMon.println("Connecting to MQTT...");
  mqttClient.connect();
}

void connectToInternet() {
  if(USE_WIFI){
    SerialMon.println("Connecting to Wi-Fi...");
    AsyncWiFiManager wifiManager(&server,&dns);
    if(wifiManager.autoConnect(ACCESS_POINT_NAME)){
      connectToMqtt();
    }
    else{
      reconnectAttempt++;
      if(reconnectAttempt >= WIFI_MAX_RECONNECT_ATTEMPTS){
        reconnectAttempt = 0;
        USE_WIFI = false;
      }
    }
  }
  else{
    SerialMon.println("Connecting to GPRS...");
    //try to connect to gprs
    SerialMon.println("Waiting for network...");
    if (modem.waitForNetwork()) {
      SerialMon.println("network present");
      vTaskDelay(pdMS_TO_TICKS(5000));
      if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
        vTaskDelay(pdMS_TO_TICKS(5000));
        // GPRS connection parameters are usually set after network registration
        SerialMon.print(F("Connecting to "));
        SerialMon.println(GPRS_APN);
        if (modem.gprsConnect(GPRS_APN, GPRS_User, GPRS_Pass)) {
          SerialMon.println("Connecting now");
          vTaskDelay(pdMS_TO_TICKS(5000));
          if (modem.isGprsConnected()) {
            SerialMon.println("GPRS connected");
            vTaskDelay(pdMS_TO_TICKS(5000));
            connectToMqtt();
          }
        }
        else{
          SerialMon.println("Connection not successful");
        }
      }
      else{
        SerialMon.println("Network NOT connected");  
      }
    }
    else{
      SerialMon.println("network NOT present");
    }
  }
}

void WiFiEvent(WiFiEvent_t event) {
    SerialMon.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        SerialMon.println("WiFi connected");
        SerialMon.println("IP address: ");
        SerialMon.println(WiFi.localIP());
        if(xTimerIsTimerActive(wifiReconnectTimer))   xTimerStop(wifiReconnectTimer, 0);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        SerialMon.println("WiFi lost connection");
        if(!xTimerIsTimerActive(wifiReconnectTimer)) wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToInternet));  
        break;
    }
}

void onMqttConnect(bool sessionPresent) {               
  SerialMon.println("Connected to MQTT.");
  SerialMon.print("Session present: ");
  SerialMon.println(sessionPresent);

  String subscribePath = BoardID + "/#";
  String logPathSTR = BoardID + "/log";
  String doorStatusPathSTR = BoardID + "/door/status";
  String jsonStatusPathSTR = BoardID + "/json";

  logPathSTR.toCharArray(logPath, logPathSTR.length() + 1);
  doorStatusPathSTR.toCharArray(doorStatusPath, doorStatusPathSTR.length() + 1);
  jsonStatusPathSTR.toCharArray(jsonStatusPath, jsonStatusPathSTR.length() + 1);

  char mess[subscribePath.length() + 1]; 
  subscribePath.toCharArray(mess, subscribePath.length() +1);
  SerialMon.print("Subscribing to ");
  SerialMon.println(mess);
  uint16_t packetIdSub1 = mqttClient.subscribe(mess, 2);      //subscribes to BoardID/#
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  SerialMon.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    connectToMqtt();
  }
}

void postStatusJson(){
  StaticJsonDocument<200> doc;

  doc["boardID"] = BoardID;
  doc["isMaster"] = isMaster;

  doc["useWifi"] = USE_WIFI;
  doc["wifiMaxReconnectAttempts"] = WIFI_MAX_RECONNECT_ATTEMPTS;

  doc["gyroscope"] = int(gy);
        
  doc["rf_enabled"] = RF_enable;
  doc["rf_password"] = RF_password;

  doc["gsm"] = modem.testAT();
  doc["admin_number"] = admin_number;
  doc["customer_number"] = customer_number;
  doc["master_number"] = master_number;
  doc["slave_number"] = slave_number;

  doc["doorway_listening"] = DOORWAY_LISTENING;
  doc["doorway_listening_time"] = timeListenDoorway;
  doc["doorway_opening_time"] = timeOpenDoorway;

  doc["door_listening"] = DOORWAY_LISTENING;
  doc["door_listening_time"] = timeListenPiezo;
  doc["door_opening_time"] = timeOpenDoor;
  doc["door_open_degree"] = SERVO_OPEN;
  doc["door_close_degree"] = SERVO_CLOSED;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  mqttClient.publish(jsonStatusPath, 0, true, jsonBuffer);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  SerialMon.println("Publish received.");
  SerialMon.print("  topic: ");
  SerialMon.println(topic);
  SerialMon.print("  qos: ");
  SerialMon.println(properties.qos);
  SerialMon.print("  dup: ");
  SerialMon.println(properties.dup);
  SerialMon.print("  retain: ");
  SerialMon.println(properties.retain);
  SerialMon.print("  len: ");
  SerialMon.println(len);
  SerialMon.print("  index: ");
  SerialMon.println(index);
  SerialMon.print("  total: ");
  SerialMon.println(total);

  String topicSTR = topic;
  
  if(topicSTR.indexOf("newBoardID") > -1){
    BoardID = "";
    for(int i = 0; i < len; i++){
      BoardID += payload[i];
    }
    SerialMon.print("Received new BoardID = ");
    SerialMon.println(BoardID);
    preferences.begin("ID", false);
    preferences.putString("BoardID", BoardID);
    preferences.end();
    ESP.restart();
  }  
  else if(topicSTR.indexOf("phone/admin") > -1){
    SerialMon.print("received new admin phone number ");
    admin_number = "";
    for(int i = 0; i < len; i++){
      admin_number += payload[i];
    }
    SerialMon.println(admin_number);
    preferences.begin("PHONE", false);
    preferences.putString("admin", admin_number);
    preferences.end();
  }
  else if(topicSTR.indexOf("phone/customer") > -1){
    SerialMon.print("received new customer phone number ");
    customer_number = "";
    for(int i = 0; i < len; i++){
      customer_number += payload[i];
    }
    SerialMon.println(customer_number);
    preferences.begin("PHONE", false);
    preferences.putString("customer", customer_number);
    preferences.end();
  }

  else if(topicSTR.indexOf("phone/call/admin") > -1){
    startCall(admin_number);
  }
  else if(topicSTR.indexOf("phone/call/customer") > -1){ 
    startCall(customer_number);
  }
  else if(topicSTR.indexOf("phone/call/number") > -1){ 
    SerialMon.println("received new number to call");
    String numberToCall = "";
    for(int i = 0; i < len; i++){
      numberToCall += payload[i];
    }
    startCall(numberToCall);
  }
  else if(topicSTR.indexOf("phone/call/hangup") > -1){ 
    hangupCall();
  }
  else if(topicSTR.indexOf("RF/enable") > -1){
      SerialMon.print("received RF enable status ");
      SerialMon.println(payload[0]);
      RF_enable = payload[0];
      preferences.begin("RF", false);
      RF_password = preferences.putBool("enable", RF_enable);
      preferences.end();
  }
  else if(topicSTR.indexOf("RF/password") > -1){
    if(payload[0]){
      SerialMon.print("received RF password ");   
      RF_password = "";
      for(int i = 0; i < len; i++){
        RF_password += payload[i];
      }
      SerialMon.println(RF_password);
      preferences.begin("RF", false);
      RF_password = preferences.putString("password", RF_password);
      preferences.end();
    }
  }
  else if(topicSTR.indexOf("door/open") > -1){
    if(payload[0] == '1'){
      SerialMon.println("received open door command");
      openDoor(timeOpenDoor);
    } 
  }
  else if(topicSTR.indexOf("door/listen") > -1){
    SerialMon.print("received door listen status ");
    SerialMon.println(payload[0]);
    if (payload[0] == '1'){
      PIEZO_LISTENING = true;
      piezoTimer = xTimerCreate("piezoTimer", pdMS_TO_TICKS(timeListenPiezo), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopPiezoListening));
      xTimerStart(piezoTimer, 0);
    }
    else stopPiezoListening();
  }
  else if(topicSTR.indexOf("door/time/open") > -1){
    String timeSTR = "";
    for(int i = 0; i < len; i++){
      timeSTR += payload[i];
    }
    int time = timeSTR.toInt();
    if(time != 0){
      SerialMon.print("received open door timer ");
      SerialMon.println(time);
      preferences.begin("TIMERS", false);
      preferences.putInt("DoorTimer", time);
      preferences.end();
      timeOpenDoor = time;
    }
  }
  else if(topicSTR.indexOf("door/time/listen") > -1){
    String timeSTR = "";
    for(int i = 0; i < len; i++){
      timeSTR += payload[i];
    }
    int time = timeSTR.toInt();
    if(time != 0){
      SerialMon.print("received listen door timer ");
      SerialMon.println(time);
      preferences.begin("TIMERS", false);
      preferences.putInt("PiezoTimer", time);
      preferences.end();
      timeListenPiezo = time;
    }
  }
  else if(topicSTR.indexOf("doorway/open") > -1){
    if (payload[0] == '1'){
      SerialMon.println("received open door command");
      openDoorway(timeOpenDoorway);
    } 
  }
  else if(topicSTR.indexOf("doorway/listen") > -1){
    SerialMon.print("received doorway listen status ");
    SerialMon.println(payload[0]);
    if (payload[0] == '1'){
      DOORWAY_LISTENING = true;
      doorwayListenTimer = xTimerCreate("doorwayListenTimer", pdMS_TO_TICKS(timeListenDoorway), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopDoorwayListening));
      xTimerStart(doorwayListenTimer, 0);
    }
    else stopDoorwayListening();
  }
  else if(topicSTR.indexOf("doorway/time/open") > -1){
    String timeSTR = "";
    for(int i = 0; i < len; i++){
      timeSTR += payload[i];
    }
    int time = timeSTR.toInt();
    if(time != 0){
      SerialMon.print("received open doorway timer ");
      SerialMon.println(time);
      preferences.begin("TIMERS", false);
      preferences.putInt("DoorwayTimer", time);
      preferences.end();
      timeOpenDoorway = time;
    }
  }
  else if(topicSTR.indexOf("doorway/time/listen") > -1){
    String timeSTR = "";
    for(int i = 0; i < len; i++){
      timeSTR += payload[i];
    }
    int time = timeSTR.toInt();
    if(time != 0){
      SerialMon.print("received listen doorway timer ");
      SerialMon.println(time);
      preferences.begin("TIMERS", false);
      preferences.putInt("DrwyListenTime", time);
      preferences.end();
      timeListenDoorway = time;
    }
  }
  else if(topicSTR.indexOf("door/servo/open") > -1){
    String degreeSTR = "";
    for(int i = 0; i < len; i++){
      degreeSTR += payload[i];
    }
    int degree = degreeSTR.toInt();
    SerialMon.print("received new servo open degree ");
    SerialMon.println(degree);
    preferences.begin("SERVO", false);
    preferences.putInt("SERVO_OPEN", degree);
    preferences.end();
    SERVO_OPEN = degree;
  }
  else if(topicSTR.indexOf("door/servo/closed") > -1){
    String degreeSTR = "";
    for(int i = 0; i < len; i++){
      degreeSTR += payload[i];
    }
    int degree = degreeSTR.toInt();
    SerialMon.print("received new servo closed degree ");
    SerialMon.println(degree);
    preferences.begin("SERVO", false);
    preferences.putInt("SERVO_CLOSED", degree);
    preferences.end();
    SERVO_CLOSED = degree;
  }
  else if(topicSTR.indexOf("wifi/enabled") > -1){
    if(payload[0] == '1') USE_WIFI = true;
    else USE_WIFI = false;
    SerialMon.print("received new USE_WIFI ");
    SerialMon.println(USE_WIFI);
    preferences.begin("WIFI", false);
    preferences.putBool("USE_WIFI", USE_WIFI);
    preferences.end();
  }
  else if(topicSTR.indexOf("wifi/reconnectAttempts") > -1){
    String attemptSTR = "";
    for(int i = 0; i < len; i++){
      attemptSTR += payload[i];
    }
    WIFI_MAX_RECONNECT_ATTEMPTS = attemptSTR.toInt();
    SerialMon.print("received new maximum wifi reconnect attempt limit");
    SerialMon.println(WIFI_MAX_RECONNECT_ATTEMPTS);
    preferences.begin("WIFI", false);
    preferences.putInt("ATTEMPT", WIFI_MAX_RECONNECT_ATTEMPTS);
    preferences.end();
  }
  else if(topicSTR.indexOf("master/isMaster") > -1){
    if(payload[0] == '1') isMaster = true;
    else isMaster = false;
    SerialMon.print("received new isMaster ");
    SerialMon.println(isMaster);
    preferences.begin("MASTER", false);
    preferences.putBool("is_master", isMaster);
    preferences.end();
  }
  else if(topicSTR.indexOf("master/masterNumber") > -1){
    master_number = "";
    for(int i = 0; i < len; i++){
      master_number += payload[i];
    }
    SerialMon.print("received new master phone number ");
    SerialMon.println(master_number);
    preferences.begin("MASTER", false);
    preferences.putString("master_number", master_number);
    preferences.end();
  }
  else if(topicSTR.indexOf("master/slaveNumber") > -1){
    SerialMon.print("received new slave phone number ");
    slave_number = "";
    for(int i = 0; i < len; i++){
      slave_number += payload[i];
    }
    SerialMon.println(slave_number);
    preferences.begin("MASTER", false);
    preferences.putString("slave_number", slave_number);
    preferences.end();
  }
  else if(topicSTR.indexOf("/json/get") > -1){
    postStatusJson();
  }
  else if(topicSTR.indexOf("/reboot") > -1){
    ESP.restart();
  }
  //vTaskDelay(pdMS_TO_TICKS(1));
}

void setup() {
  SerialMon.begin(115200);                  // Set console baud rate

  //Preferences
  preferences.begin("ID", false);
  BoardID = preferences.getString("BoardID", "");
  if(BoardID == "") preferences.putString("BoardID", "1");
  preferences.end();
  
  SerialMon.print("Booting up board with id: ");
  SerialMon.println(BoardID);
  
  preferences.begin("MASTER", false);
  isMaster = preferences.getBool("is_master", true);
  master_number = preferences.getString("master_number", "");
  slave_number = preferences.getString("slave_number", "");
  preferences.end();

  preferences.begin("WIFI", false);
  USE_WIFI = preferences.getBool("USE_WIFI", true);
  WIFI_MAX_RECONNECT_ATTEMPTS = preferences.getInt("ATTEMPT", 3);
  preferences.end();

  preferences.begin("SERVO", false);
  SERVO_OPEN = preferences.getInt("SERVO_OPEN", 0);
  SERVO_CLOSED = preferences.getInt("SERVO_CLOSED", 180);
  preferences.end();

  preferences.begin("RF", false);
  RF_password = preferences.getString("password", "");
  RF_enable = preferences.getBool("enable", false);
  if(RF_password == "") preferences.putString("password", "password");
  preferences.end();

  preferences.begin("PHONE", false);
  admin_number = preferences.getString("admin", "");
  customer_number = preferences.getString("customer", "");
  if(admin_number == "") preferences.putString("admin", DEFAULT_ADMIN_PHONE);
  if(customer_number == "") preferences.putString("customer", DEFAULT_CUSTOMER_PHONE);
  preferences.end();

  preferences.begin("TIMERS", false);
  timeListenPiezo = preferences.getInt("PiezoTimer", 5000);
  timeOpenDoor = preferences.getInt("DoorTimer", 5000);
  timeOpenDoorway = preferences.getInt("DoorwayTimer", 5000);
  timeListenDoorway = preferences.getInt("DrwyListenTime", 5000);
  preferences.end();
  
  //Setup Door pins
  pinMode(PIEZO_PIN, INPUT);
  pinMode(DOORWAY_IN_PIN, INPUT);
  pinMode(DOORWAY_OUT_PIN, OUTPUT);

  digitalWrite(DOORWAY_OUT_PIN, LOW);

  //Setup servo
  ESP32PWM::allocateTimer(0);
  doorServo.setPeriodHertz(50);
	doorServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US); 
  doorServo.write(SERVO_CLOSED);
  
  //SETUP RF
  if (!driver.init()) SerialMon.println("ERROR - RF setup failed ");
  else SerialMon.println("RF setup completed");


  // Start power management
  if (setupPMU() == false) {
      SerialMon.println("Setting power error");
  }
  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
 

  simModuleSetup(false);
    //SETUP I2C for MPU6050
  Wire.begin(I2C_SDA_2, I2C_SCL_2);
  accelgyro.initialize();
  SerialMon.println("Testing MPU6050 connections...");
  SerialMon.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
      
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToInternet();

  TaskHandle_t xHandle1 = NULL;
  TaskHandle_t xHandle2 = NULL;
  TaskHandle_t xHandle3 = NULL;
  TaskHandle_t xHandle4 = NULL;
  TaskHandle_t xHandle5 = NULL;
  
  BaseType_t xReturned1;
  BaseType_t xReturned2;
  BaseType_t xReturned3;
  BaseType_t xReturned4;
  BaseType_t xReturned5;
  
  //Create Task for listening to Door, Doorway bell and RF
  xReturned1 = xTaskCreate(listenPiezo, "Listen Piezo", 8000, (void*)0, 2, &xHandle1);
  xReturned2 = xTaskCreate(listenDoorway, "Listen Doorway", 8000, (void*)0, 3, &xHandle2);
  xReturned3 = xTaskCreate(listenRF, "Listen RF", 8000, (void*)0, 4, &xHandle3);
  xReturned4 = xTaskCreate(listenIncomingCall, "Listen IncomingCall", 8000, (void*)0, 5, &xHandle4);
  xReturned5 = xTaskCreate(listenGyro, "Listen Gyro", 8000, (void*)0, 4, &xHandle5);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}