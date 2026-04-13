#ifndef CONFIG_H
#define CONFIG_H

// WiFi
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"

// MQTT
#define MQTT_HOST "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USER ""           // leave empty if no auth
#define MQTT_PASS ""
#define MQTT_CLIENT_ID "jarvis-desk"

// Desk UART pins (ESP32-C6)
#define DESK_RX_PIN 17   // ESP32 RX ← desk DTX
#define DESK_TX_PIN 18   // ESP32 TX → desk HTX
#define DESK_UART_NUM 1  // UART1

// OTA
#define OTA_HOSTNAME "jarvis-desk"
#define OTA_PASSWORD ""  // leave empty to disable OTA password

#endif
