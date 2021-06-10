
void listenRF(void* pvParameters)
{
    for (;;) {
        uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
        uint8_t buflen = sizeof(buf);
        if (driver.recv(buf, &buflen))          //If got anything from RF receiver
        {
            String message;
            for (int i = 0; i <= buflen; i++){
                message += char(buf[i]);
            }
            SerialMon.print("Received RF signal width code: ");
            SerialMon.println(message);
            if(message.indexOf(RF_password) > -1){
                SerialMon.println("Password matching");
                if(RF_enable){
                    SerialMon.println("RF is enabled, opening door");
                    mqttClient.publish(logPath, 2, true, "Got right RF password, opening the door");
                    openDoor(timeOpenDoor);
                }
                else{
                    SerialMon.println("RF is disabled tho");
                    mqttClient.publish(logPath, 2, true, "Got right RF password, but RF door opening is disabled");
                }
            }
            else{
                SerialMon.println("Passord NOT matching");
                mqttClient.publish(logPath, 2, true, "Got wrong RF password");
            }
        }
        vTaskDelay(1);
    }
}