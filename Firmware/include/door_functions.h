
bool highDoorwayStatus = false;
bool highPiezoStatus = false;


void stopPiezoListening(){
    SerialMon.println("Stopped piezo listening");
    mqttClient.publish(logPath, 0, false, "Stopped piezo listening");
    PIEZO_LISTENING = false;
}

void stopServo(void* pvParameters)
{ 
    doorServo.write(SERVO_CLOSED);
    SerialMon.println("Stopped servo!");
    mqttClient.publish(logPath, 0, false, "Stopped servo!");
    stopPiezoListening();
}

void openDoor(int ms){
    SerialMon.println("Opening door!");
    mqttClient.publish(logPath, 0, false, "Opening door!"); 
	doorServo.write(SERVO_OPEN);
    doorTimer = xTimerCreate("doorTimer", pdMS_TO_TICKS(ms), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopServo));
    xTimerStart(doorTimer, 0);
}

void listenPiezo(void* pvParameters)
{
    printf("%s\n", "listenPiezo");
    for (;;)
    {
        int reading = analogRead(PIEZO_PIN);
        if(reading >= PIEZO_THRESHOLD && PIEZO_LISTENING && !highPiezoStatus){
            Serial.println(reading);
            SerialMon.println("Piezo Triggered, opening door!");
            mqttClient.publish(logPath, 0, false, "Piezo Triggered, opening door!");
            openDoor(timeOpenDoor);
            highPiezoStatus = true;
        }
        else if(reading < PIEZO_THRESHOLD && !PIEZO_LISTENING){
            highPiezoStatus = false;
        }
        vTaskDelay(1);
    }
}

void stopDoorwayListening(){
    SerialMon.println("Stopped doorway listening");
    mqttClient.publish(logPath, 0, false, "Stopped doorway listening");
    DOORWAY_LISTENING = false;
}

void stopDoorway(){
    digitalWrite(DOORWAY_OUT_PIN, LOW);    
    SerialMon.println("Stopped doorway!");
    mqttClient.publish(logPath, 0, false, "Stopped doorway");
    stopDoorwayListening();
}

void openDoorway(int ms){
    SerialMon.println("Opening doorway!");
    mqttClient.publish(logPath, 0, false, "Opening doorway");
    digitalWrite(DOORWAY_OUT_PIN, HIGH);    
    doorwayOutTimer = xTimerCreate("doorwayOutTimer", pdMS_TO_TICKS(ms), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopDoorway));
    xTimerStart(doorwayOutTimer, 0);
}


void checkEmergency(){
    if(digitalRead(DOORWAY_IN_PIN && highDoorwayStatus)){
        SerialMon.println("Emergency pattern triggered, calling admin phone");
        mqttClient.publish(logPath, 0, false, "Emergency pattern triggered, calling admin phone"); 
        startCall(admin_number);
    }
}


void listenDoorway(void* pvParameters)
{
    emergencyCallTimer = xTimerCreate("emergencyTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(checkEmergency));
    printf("%s\n", "listenDoorway");
    for (;;)
    {
        if(digitalRead(DOORWAY_IN_PIN)){
        //if(analogRead(DOORWAY_IN_PIN) >= DOORWAY_THRESHOLD){  //to be used when adc is connected
            if(!highDoorwayStatus){
                highDoorwayStatus = true;
                if(DOORWAY_LISTENING){
                    SerialMon.println("Doorway bell triggered, opening doorway!");
                    mqttClient.publish(logPath, 0, false, "Doorway bell triggered, opening doorway!");
                    openDoorway(timeOpenDoorway);
                }
                else if (EMERGENCY_CALL){
                    SerialMon.println("Doorway bell triggered, hang up emergency call");
                    mqttClient.publish(logPath, 0, false, "Doorway bell triggered, hang up emergency call");
                    hangupCall();
                }
                else{
                    if(!xTimerIsTimerActive(emergencyCallTimer)){
                        xTimerStart(emergencyCallTimer, 0);
                    }
                    SerialMon.println("Doorway bell is ringing, maybe is an emergency pattern?");
                    mqttClient.publish(logPath, 0, false, "Doorway bell is ringing, maybe is an emergency pattern?"); 
                }
            }
        }
        else{
            highDoorwayStatus = false;
            if(xTimerIsTimerActive(emergencyCallTimer)) xTimerStop(emergencyCallTimer, 0);
        }
        vTaskDelay(1);
    }
}
