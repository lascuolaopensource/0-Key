#Firmware


## Files format

The project has been developed using PlatformIO IDE for Visual Studio Code.
Building and uploading should be straightforward, with automatic dependencies collection-


## Dependencies

This source depends on several libraries in order to build correctly:

- [AsyncMqttClient](https://github.com/marvinroger/async-mqtt-client) 
- [ESPAsyncWifiManager](https://github.com/alanswx/ESPAsyncWiFiManager)
- [ESP32Servo](https://github.com/jkb-git/ESP32Servo)
- [RadioHead](https://github.com/PaulStoffregen/RadioHead)
- [MPU6050](https://github.com/ElectronicCats/mpu6050)
- [TinyGSM](https://github.com/vshymanskyy/TinyGSM)
- [FreeRTOS](https://www.freertos.org/)   


## Current status

The codebase needs details implementations in order to be completely functional:

    - Phone call interaction (for both admin and customer) post number checking need to be implemented.
    - RF interaction interaction post password checking needs to be implemented.
    - Accelerometer interaction needs to be reviewed and tested.
    - Emergency patterns need to be reviewed and tested. 
