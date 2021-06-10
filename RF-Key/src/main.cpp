#include <Arduino.h>
#include <RH_ASK.h>
#ifdef RH_HAVE_HARDWARE_SPI
    #include <SPI.h> // Not actually used but needed to compile
#endif

RH_ASK driver(2000, 4, 5, 0);

void setup()
{
    Serial.begin(115200);
    if (!driver.init()) 
        Serial.println("RF init failed");
    else 
        Serial.println("RF init completed");
}

void loop()
{
    const char *msg = "password";

    driver.send((uint8_t *)msg, strlen(msg));
    driver.waitPacketSent();
    delay(200);
    Serial.println("password sent!");
}