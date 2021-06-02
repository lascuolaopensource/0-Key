## Files format

The project has been developed using PlatformIO IDE for Visual Studio Code.
Building and uploading should be straightforward, with automatic dependencies collection-


## Current status

The codebase needs details implementations in order to be completely functional:

    - Phone call interaction (for both admin and customer) post number checking need to be implemented.
    - RF interaction interaction post password checking needs to be implemented.
    - Accelerometer interaction needs to be reviewed and tested.
    - Emergency patterns need to be reviewed and tested. 


## Dependencies

This source depends on several libraries in order to build correctly:

    - AsyncMqttClient 
    - ESPAsyncWifiManager
    - ESP32Servo (servomotor)
    - RadioHead (RF Modules)
    - MPU6050
    - TinyGSM
    - FreeRTOS (Real Time Operative Sistem framework)   