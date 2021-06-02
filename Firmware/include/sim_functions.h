
void setupCall(){
    SerialAT.print("AT+CHFA=1\r\n");
    //vTaskDelay(pdMS_TO_TICKS(2));
    delay(2);
    //Set ringer sound level
    SerialAT.print("AT+CRSL=100\r\n");
    //vTaskDelay(pdMS_TO_TICKS(2));
    delay(2);
    //Set loud speaker volume level
    SerialAT.print("AT+CLVL=100\r\n");
    //vTaskDelay(pdMS_TO_TICKS(2));
    delay(2);
    // Calling line identification presentation
    SerialAT.print("AT+CLIP=1\r\n");
    //vTaskDelay(pdMS_TO_TICKS(2));
    delay(2);
    //Set RI Pin input
    pinMode(MODEM_RI, INPUT);
}

void setupModem()
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
    
    //vTaskDelay(pdMS_TO_TICKS(10));
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    
    //vTaskDelay(pdMS_TO_TICKS(10));
    delay(1000);
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

void simModuleSetup(){
    // Some start operations
    SerialMon.println("Initializing modem...");
    setupModem();

    modem.restart();
    turnOffNetlight();
    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem: ");
    SerialMon.println(modemInfo);
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
    }
    
    modem.sleepEnable();
    //vTaskDelay(pdMS_TO_TICKS(1000));
    delay(1000);
    bool res;
    res = modem.testAT();
    SerialMon.print("SIM800 Test AT result -> ");
    SerialMon.println(res);
    
    SerialMon.println("Use DTR Pin Wakeup");
    pinMode(MODEM_DTR, OUTPUT);
    //Set DTR Pin low , wakeup modem .
    digitalWrite(MODEM_DTR, LOW);

    // test modem response , res == 1 , modem is wakeup
    res = modem.testAT();
    SerialMon.print("SIM800 Test AT result -> ");
    SerialMon.println(res);

    setupCall();
    SerialAT.flush();
}

void listenIncomingCall(void* pvParameters)
{
    SerialMon.println("Sim setup completed, waiting for call in");
    mqttClient.publish("log/", 0, false, "Sim setup completed, waiting for call in");
    for (;;) {
        if(!digitalRead(MODEM_RI)) {  
            SerialMon.println("call in ");
            
            String callingNumber;
            
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            SerialMon.println("Hangup");
            SerialAT.println("ATH");

            while(SerialAT.available() > 0){
            char as = SerialAT.read();
            callingNumber += as;
            }

            if(callingNumber.indexOf('+CLIP') > 0){
                callingNumber = callingNumber.substring(callingNumber.indexOf('+CLIP'));
                String msg = "Receiving a phone call from ";
                if(callingNumber.indexOf(admin_number) > -1){
                    callingNumber = callingNumber.substring(callingNumber.indexOf(admin_number), callingNumber.indexOf(admin_number) + 14);
                    msg += "admin, will do something";
                }
                else if(callingNumber.indexOf(customer_number) > -1){
                    callingNumber = callingNumber.substring(callingNumber.indexOf(customer_number), callingNumber.indexOf(customer_number) + 14);
                    msg += "customer, will do something else";
                }
                else{
                    msg += "someone else, won't do anything";
                }
                char mess[100];
                msg.toCharArray(mess, 100);
                SerialMon.println(mess);
                mqttClient.publish("log/", 2, true, mess);
                
                vTaskDelay(pdMS_TO_TICKS(1000));
            
                simModuleSetup();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
