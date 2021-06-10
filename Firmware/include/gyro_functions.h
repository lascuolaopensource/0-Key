void listenGyro(void* pvParameters)
{    
    bool gyroTriggered = false;
    for (;;){
        accelgyro.getRotation(&gx, &gy, &gz);
        if (int(gy) > int(GYRO_THRESHOLD) || int(gy) < -int(GYRO_THRESHOLD)){
            if(!gyroTriggered){
                gyroTriggered = true;
                SerialMon.print("Door moving ");
                SerialMon.print(gx); SerialMon.println("\t");  // questo!!
                //SerialMon.print(gy); SerialMon.print("\t");
                //SerialMon.println(gz);
                mqttClient.publish(doorStatusPath, 0, false, "Door moving!");
            }
        }
        else if (int(gy) < int(GYRO_THRESHOLD) && int(gy) >  - int(GYRO_THRESHOLD)) gyroTriggered = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}