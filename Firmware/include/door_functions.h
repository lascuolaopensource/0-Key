
void stopServo(void* pvParameters)
{
    for (int pos = SERVO_OPEN; pos >= SERVO_CLOSED; pos *= 1) {
		doorServo.write(pos);
        vTaskDelay(pdMS_TO_TICKS(1));
	}
    SerialMon.println("Stopped servo!");
    mqttClient.publish(logPath, 0, false, "Stopped servo!");
    PIEZO_LISTENING = false;
}

void openDoor(int ms){
    SerialMon.println("Opening door!");
    mqttClient.publish(logPath, 0, false, "Opening door!");
	for (int pos = SERVO_CLOSED; pos <= SERVO_OPEN; pos += 1) {
		doorServo.write(pos);
        vTaskDelay(pdMS_TO_TICKS(1));
	}
    doorTimer = xTimerCreate("doorTimer", pdMS_TO_TICKS(ms), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopServo));
    xTimerStart(doorTimer, 0);
}

void listenPiezo(void* pvParameters)
{
    bool highPiezoStatus = false;
    printf("%s\n", "listenPiezo");
    for (;;)
    {
        if(PIEZO_LISTENING){
            int reading = analogRead(PIEZO_PIN);
            
            if(reading >= PIEZO_THRESHOLD && highPiezoStatus == false){
                Serial.println(reading);
                SerialMon.println("Piezo Triggered, opening door!");
                mqttClient.publish(logPath, 0, false, "Piezo Triggered, opening door!");
                openDoor(defaultDoorTimer);
                highPiezoStatus = true;
            }
        }
        else highPiezoStatus = false;
        vTaskDelay(1);
    }
}

void stopPiezoListening(){
    SerialMon.println("Stopped piezo listening");
    mqttClient.publish(logPath, 0, false, "Stopped piezo listening");
    PIEZO_LISTENING = false;
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
    DOORWAY_LISTENING = false;
}

void openDoorway(int ms){
    SerialMon.println("Opening doorway!");
    mqttClient.publish(logPath, 0, false, "Opening doorway");
    digitalWrite(DOORWAY_OUT_PIN, HIGH);    
    doorwayOutTimer = xTimerCreate("doorwayOutTimer", pdMS_TO_TICKS(ms), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(stopDoorway));
    xTimerStart(doorwayOutTimer, 0);
}

void listenDoorway(void* pvParameters)
{
    bool highDoorwayStatus = false;
    printf("%s\n", "listenDoorway");
    for (;;)
    {
        if(DOORWAY_LISTENING){
            if(digitalRead(DOORWAY_IN_PIN) && !highDoorwayStatus){
                SerialMon.println("Doorway bell triggered, opening doorway!");
                mqttClient.publish(logPath, 0, false, "Doorway bell triggered, opening doorway!");
                openDoorway(defaultDoorwayTimer);
                highDoorwayStatus = true;
            } 
        }
        else highDoorwayStatus = false;
        vTaskDelay(1);
    }
}
