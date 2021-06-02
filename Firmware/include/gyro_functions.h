void listenGyro(void* pvParameters)
{    
    bool gyroTriggered = false;
    for (;;){
        accelgyro.getRotation(&gx, &gy, &gz);
        if (gy >= GYRO_THRESHOLD && !gyroTriggered){
            gyroTriggered = true;
            SerialMon.print("a/g:\t");
            SerialMon.print(gx); SerialMon.print("\t");  // questo!!
            SerialMon.print(gy); SerialMon.print("\t");
            SerialMon.println(gz);
            mqttClient.publish("door/status", 0, false, "Door moving!");
        }
        else if (gy < GYRO_THRESHOLD) gyroTriggered = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}