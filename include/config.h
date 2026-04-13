#ifndef CONFIG_H
#define CONFIG_H

// WiFi
#define WIFI_SSID "TheSprinkleHouse"
#define WIFI_PASS "Tabl3T0p"

// MQTT
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883
#define MQTT_USER "home-mosquitto"
#define MQTT_PASS "mnb1kuh-ptr8eat-WUB"
#define MQTT_CLIENT_ID "jarvis-desk"

// Desk UART pins (ESP32-C6)
#define DESK_RX_PIN 17   // ESP32 RX ← desk DTX (Beetle pin 13, GPIO17)
#define DESK_TX_PIN 16   // ESP32 TX → desk HTX (Beetle pin 12, GPIO16)
#define DESK_UART_NUM 1

// OTA
#define OTA_HOSTNAME "jarvis-desk"
#define OTA_PASSWORD ""

#endif
