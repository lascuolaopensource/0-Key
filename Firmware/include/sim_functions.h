bool HANGIN_UP = false;

void setupCall(bool isTask){
    SerialAT.print("AT+CHFA=1\r\n");
    if(isTask) vTaskDelay(pdMS_TO_TICKS(2));
    else delay(2);
    //Set ringer sound level
    SerialAT.print("AT+CRSL=100\r\n");
    if(isTask) vTaskDelay(pdMS_TO_TICKS(2));
    else delay(2);
    //Set loud speaker volume level
    SerialAT.print("AT+CLVL=100\r\n");
    if(isTask) vTaskDelay(pdMS_TO_TICKS(2));
    else delay(2);
    // Calling line identification presentation
    SerialAT.print("AT+CLIP=1\r\n");
    if(isTask) vTaskDelay(pdMS_TO_TICKS(2));
    else delay(2);
    //Set RI Pin input
    pinMode(MODEM_RI, INPUT);
}

void setupModem(bool isTask)
{
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    
    if(isTask)vTaskDelay(pdMS_TO_TICKS(100));
    else delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    
    if(isTask)vTaskDelay(pdMS_TO_TICKS(1000));
    else delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF);
}

void turnOffNetlight()
{
    SerialMon.println("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    SerialMon.println("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}

void simModuleSetup(bool isTask){          //Main Sim module init
    SerialMon.println("Initializing modem...");
    setupModem(isTask);

    modem.restart();
    turnOffNetlight();
    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem: ");
    SerialMon.println(modemInfo);
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
        modem.simUnlock(simPIN);
    }
    modem.sleepEnable();
    if(isTask)vTaskDelay(pdMS_TO_TICKS(1000));
    else delay(1000);

    pinMode(MODEM_DTR, OUTPUT);
    digitalWrite(MODEM_DTR, LOW);       //Set DTR Pin low , wakeup modem .

    setupCall(isTask);                        //Send commands for call input handling
    SerialAT.flush();

    bool res = modem.testAT();          // test modem response , res == 1 , modem is wakeup
    SerialMon.print("SIM800 Test AT result -> ");
    SerialMon.println(res);
    if(res == 1){
        SerialMon.println("Sim setup completed, waiting for call in");
        mqttClient.publish(logPath, 0, false, "Sim setup completed, waiting for call in");
    }
    else{
        SerialMon.println("Sim error");
        mqttClient.publish(logPath, 0, false, "Sim error");
    }
    HANGIN_UP = false;
}

void listenIncomingCall(void* pvParameters)     //main call listening task
{ 
    for (;;) {
        if(!digitalRead(MODEM_RI) && !HANGIN_UP && !EMERGENCY_CALL) {                //Ring pin down, someone is calling
            SerialMon.println("call in ");
            String callingNumber;
            vTaskDelay(pdMS_TO_TICKS(1000));         
            SerialMon.println("Hangup");
            SerialAT.println("ATH");

            while(SerialAT.available() > 0){
            char as = SerialAT.read();
            callingNumber += as;
            }
            SerialAT.flush();
            if(callingNumber.indexOf('RING') > 0)
                callingNumber = callingNumber.substring(callingNumber.indexOf('RING'));
            if(callingNumber.indexOf('+CLIP') > 0){
                callingNumber = callingNumber.substring(callingNumber.indexOf('+CLIP: "'));
                String msg = "Receiving a phone call from ";
                if(callingNumber.indexOf(admin_number) > -1){
                    callingNumber = callingNumber.substring(callingNumber.indexOf(admin_number), callingNumber.indexOf(admin_number) + 14);
                    if(isMaster){
                        msg += "admin. This board is master, will call slaves";
                        //call slaves
                    }
                    else{
                        msg += "admin, will do something";
                        //listen doorway / door
                    }
                }
                else if(callingNumber.indexOf(customer_number) > -1){
                    callingNumber = callingNumber.substring(callingNumber.indexOf(customer_number), callingNumber.indexOf(customer_number) + 14);
                    msg += "customer, will do something else";
                }
                else if(!isMaster && callingNumber.indexOf(master_number) > -1){
                    callingNumber = callingNumber.substring(callingNumber.indexOf(master_number), callingNumber.indexOf(master_number) + 14);
                    msg += "master board. This board is slave, will do something";
                    //listen doorway / door
                }
                else{
                    SerialMon.println(callingNumber);
                    msg += callingNumber.substring(1, 14);
                    msg += ", won't do anything";
                }
                char mess[100];
                msg.toCharArray(mess, 100);
                SerialMon.println(mess);
                mqttClient.publish(logPath, 0, true, mess);
                vTaskDelay(pdMS_TO_TICKS(1000));
                simModuleSetup(true);
                SerialAT.flush();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void startCall(String number){ 
    String msg = "Calling " + number;
    char mess[100];
    msg.toCharArray(mess, 100);
    bool res = modem.callNumber(number);
    vTaskDelay(pdMS_TO_TICKS(1000));
    SerialMon.print("Call:");
    SerialMon.println(res ? "OK" : "fail");
    if(!res){
        EMERGENCY_CALL = false;
        mqttClient.publish(logPath, 0, false, "Error while calling");
    }
    else{
        EMERGENCY_CALL = true;
        mqttClient.publish(logPath, 0, false, mess);
    }
}

void hangupCall(){
    EMERGENCY_CALL = false;
    HANGIN_UP = true;
    bool res = modem.callHangup();
    SerialMon.print("Hang up:");
    SerialMon.println(res ? "OK" : "fail");
    simModuleSetup(true);                           //reset module after hangup
}