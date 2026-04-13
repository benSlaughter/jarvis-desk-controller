#ifdef ESP32
#include <Arduino.h>
#include <WiFi.h>
#include "jarvis_desk.h"
#include "config.h"

HardwareSerial deskSerial(DESK_UART_NUM);
JarvisDesk desk(&deskSerial);

void setup() {
    Serial.begin(115200);
    Serial.println("Jarvis Desk Controller — ESP32-C6");
    Serial.println("WiFi + MQTT version (skeleton)");

    deskSerial.begin(9600, SERIAL_8N1, DESK_RX_PIN, DESK_TX_PIN);
    desk.begin();
}

void loop() {
    desk.update();
}
#endif
