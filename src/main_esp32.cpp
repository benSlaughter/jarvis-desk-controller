#ifdef ESP32
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "jarvis_desk.h"
#include "config.h"

// ── Hardware ────────────────────────────────────────────────────────
HardwareSerial deskSerial(DESK_UART_NUM);
JarvisDesk desk(&deskSerial);

// ── Network ─────────────────────────────────────────────────────────
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ── MQTT topics ─────────────────────────────────────────────────────
static const char* TOPIC_STATUS       = "jarvis-desk/status";
static const char* TOPIC_HEIGHT_STATE = "jarvis-desk/height/state";
static const char* TOPIC_HEIGHT_SET   = "jarvis-desk/height/set";
static const char* TOPIC_PRESET_SET   = "jarvis-desk/preset/set";
static const char* TOPIC_PRESET_SAVE  = "jarvis-desk/preset/save";
static const char* TOPIC_COMMAND      = "jarvis-desk/command";
static const char* TOPIC_COLLISION    = "jarvis-desk/collision/set";

// ── Software version ────────────────────────────────────────────────
static const char* SW_VERSION = "1.0.0";

// ── WiFi state ──────────────────────────────────────────────────────
static unsigned long wifiLastAttempt = 0;
static const unsigned long WIFI_RETRY_MS = 5000;

// ── MQTT state ──────────────────────────────────────────────────────
static unsigned long mqttLastAttempt = 0;
static unsigned long mqttBackoff = 1000;
static const unsigned long MQTT_MAX_BACKOFF = 30000;

// ── Height publishing state ─────────────────────────────────────────
static uint16_t lastPublishedHeight = 0;
static unsigned long lastHeightPublishTime = 0;
static bool heightSettled = true;
static const unsigned long HEIGHT_PUBLISH_INTERVAL = 500;

// ── Startup query state ─────────────────────────────────────────────
static bool startupQueriesSent = false;
static unsigned long deskConnectedTime = 0;

// ── Simulator state ─────────────────────────────────────────────────
static bool simMode = false;

// ── Callbacks ───────────────────────────────────────────────────────

void onDeskPacket(const JarvisPacket& pkt) {
    bool fromController = (pkt.address == JARVIS_ADDR_CONTROLLER);
    const char* source = fromController ? "DESK" : "HS";
    const char* name = jarvis_command_name(pkt.command, fromController);

    Serial.printf("[%s] %s", source, name);
    if (pkt.length > 0) {
        Serial.print(" (");
        for (uint8_t i = 0; i < pkt.length; i++) {
            if (i > 0) Serial.print(" ");
            Serial.printf("%02X", pkt.params[i]);
        }
        Serial.print(")");
    }
    if (fromController && pkt.command == RESP_HEIGHT && pkt.length >= 2) {
        uint16_t h = jarvis_decode_height(pkt);
        Serial.printf("  %u.%ucm", h / 10, h % 10);
    }
    if (fromController && pkt.command == RESP_PHYS_LIMITS && pkt.length >= 4) {
        uint16_t pmin = ((uint16_t)pkt.params[0] << 8) | pkt.params[1];
        uint16_t pmax = ((uint16_t)pkt.params[2] << 8) | pkt.params[3];
        Serial.printf("  min=%umm max=%umm", pmin, pmax);
    }
    Serial.println();
}

void onMoveTimeout() {
    Serial.println("! SAFETY TIMEOUT — movement stopped after 30s");
}

void onTargetReached(uint16_t height, bool reached) {
    if (reached) {
        Serial.printf("> Target height reached: %umm\n", height);
    } else {
        Serial.println("! Move-to-height stopped: no height reports (safety timeout)");
    }
}

// ── WiFi ────────────────────────────────────────────────────────────

void handleWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - wifiLastAttempt < WIFI_RETRY_MS) return;
    wifiLastAttempt = now;

    Serial.printf("WiFi: connecting to %s...\n", WIFI_SSID);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// ── HA Discovery ────────────────────────────────────────────────────

static void addDeviceBlock(JsonObject dev) {
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add("jarvis_desk");
    dev["name"] = "Jarvis Desk";
    dev["mdl"] = "Fully Jarvis (Jiecang)";
    dev["mf"] = "Fully/Jiecang";
    dev["sw"] = SW_VERSION;
}

void publishDiscovery() {
    char buf[512];

    // 1. Height sensor
    {
        JsonDocument doc;
        doc["name"] = "Desk Height";
        doc["uniq_id"] = "jarvis_desk_height";
        doc["stat_t"] = TOPIC_HEIGHT_STATE;
        doc["unit_of_meas"] = "cm";
        doc["ic"] = "mdi:desk";
        doc["val_tpl"] = "{{ (value | float / 10) | round(1) }}";
        addDeviceBlock(doc["dev"].to<JsonObject>());
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish("homeassistant/sensor/jarvis_desk/height/config", buf, true);
    }

    // 2. Target height number
    {
        JsonDocument doc;
        doc["name"] = "Target Height";
        doc["uniq_id"] = "jarvis_desk_target_height";
        doc["cmd_t"] = TOPIC_HEIGHT_SET;
        doc["stat_t"] = TOPIC_HEIGHT_STATE;
        doc["unit_of_meas"] = "mm";
        doc["step"] = 1;
        doc["min"] = desk.hasPhysicalLimits() ? desk.getPhysicalMin() : 620;
        doc["max"] = desk.hasPhysicalLimits() ? desk.getPhysicalMax() : 1270;
        doc["ic"] = "mdi:arrow-up-down";
        addDeviceBlock(doc["dev"].to<JsonObject>());
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish("homeassistant/number/jarvis_desk/target_height/config", buf, true);
    }

    // 3. Preset buttons
    for (int i = 1; i <= 4; i++) {
        JsonDocument doc;
        char nameStr[20];
        char uidStr[30];
        char topicStr[60];
        snprintf(nameStr, sizeof(nameStr), "Preset %d", i);
        snprintf(uidStr, sizeof(uidStr), "jarvis_desk_preset_%d", i);
        snprintf(topicStr, sizeof(topicStr), "homeassistant/button/jarvis_desk/preset_%d/config", i);

        doc["name"] = nameStr;
        doc["uniq_id"] = uidStr;
        doc["cmd_t"] = TOPIC_PRESET_SET;
        char payload[2];
        snprintf(payload, sizeof(payload), "%d", i);
        doc["pl_prs"] = payload;
        doc["ic"] = "mdi:numeric";
        addDeviceBlock(doc["dev"].to<JsonObject>());
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish(topicStr, buf, true);
    }

    // 4. Save Preset buttons
    for (int i = 1; i <= 4; i++) {
        JsonDocument doc;
        char nameStr[24];
        char uidStr[36];
        char topicStr[72];
        snprintf(nameStr, sizeof(nameStr), "Save Preset %d", i);
        snprintf(uidStr, sizeof(uidStr), "jarvis_desk_save_preset_%d", i);
        snprintf(topicStr, sizeof(topicStr), "homeassistant/button/jarvis_desk/save_preset_%d/config", i);

        doc["name"] = nameStr;
        doc["uniq_id"] = uidStr;
        doc["cmd_t"] = TOPIC_PRESET_SAVE;
        char payload[2];
        snprintf(payload, sizeof(payload), "%d", i);
        doc["pl_prs"] = payload;
        doc["ic"] = "mdi:content-save";
        addDeviceBlock(doc["dev"].to<JsonObject>());
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish(topicStr, buf, true);
    }

    // 5. Up / Down / Stop buttons
    struct { const char* name; const char* id; const char* payload; const char* icon; } buttons[] = {
        {"Raise",  "up",   "up",   "mdi:arrow-up"},
        {"Lower",  "down", "down", "mdi:arrow-down"},
        {"Stop",   "stop", "stop", "mdi:stop"},
    };
    for (auto& b : buttons) {
        JsonDocument doc;
        char uidStr[30];
        char topicStr[60];
        snprintf(uidStr, sizeof(uidStr), "jarvis_desk_%s", b.id);
        snprintf(topicStr, sizeof(topicStr), "homeassistant/button/jarvis_desk/%s/config", b.id);

        doc["name"] = b.name;
        doc["uniq_id"] = uidStr;
        doc["cmd_t"] = TOPIC_COMMAND;
        doc["pl_prs"] = b.payload;
        doc["ic"] = b.icon;
        addDeviceBlock(doc["dev"].to<JsonObject>());
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish(topicStr, buf, true);
    }

    Serial.println("MQTT: HA discovery published");
}

// ── MQTT callback ───────────────────────────────────────────────────

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.printf("MQTT rx [%s]: %s\n", topic, msg.c_str());

    if (strcmp(topic, TOPIC_HEIGHT_SET) == 0) {
        long target = msg.toInt();
        if (target > 0) {
            Serial.printf("> MQTT goto %ldmm\n", target);
            desk.moveToHeight((uint16_t)target);
        }
    } else if (strcmp(topic, TOPIC_PRESET_SET) == 0) {
        int preset = msg.toInt();
        if (preset >= 1 && preset <= 4) {
            Serial.printf("> MQTT preset %d\n", preset);
            desk.moveToPreset((uint8_t)preset);
        }
    } else if (strcmp(topic, TOPIC_PRESET_SAVE) == 0) {
        int preset = msg.toInt();
        if (preset >= 1 && preset <= 4) {
            Serial.printf("> MQTT save preset %d\n", preset);
            desk.savePreset(preset);
        }
    } else if (strcmp(topic, TOPIC_COMMAND) == 0) {
        if (msg == "up") {
            Serial.println("> MQTT raise");
            desk.startRaise();
        } else if (msg == "down") {
            Serial.println("> MQTT lower");
            desk.startLower();
        } else if (msg == "stop") {
            Serial.println("> MQTT stop");
            desk.stop();
        }
    } else if (strcmp(topic, TOPIC_COLLISION) == 0) {
        if (msg == "high") {
            desk.setCollisionSensitivity(COLL_HIGH);
        } else if (msg == "medium") {
            desk.setCollisionSensitivity(COLL_MEDIUM);
        } else if (msg == "low") {
            desk.setCollisionSensitivity(COLL_LOW);
        }
    }
}

// ── MQTT connect ────────────────────────────────────────────────────

void handleMqtt() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (mqtt.connected()) {
        mqtt.loop();
        return;
    }

    unsigned long now = millis();
    if (now - mqttLastAttempt < mqttBackoff) return;
    mqttLastAttempt = now;

    Serial.printf("MQTT: connecting to %s:%d...\n", MQTT_HOST, MQTT_PORT);

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setCallback(mqttCallback);

    bool ok;
    if (strlen(MQTT_USER) > 0) {
        ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                          TOPIC_STATUS, 1, true, "offline");
    } else {
        ok = mqtt.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                          TOPIC_STATUS, 1, true, "offline");
    }

    if (ok) {
        Serial.println("MQTT: connected");
        mqttBackoff = 1000;

        mqtt.publish(TOPIC_STATUS, "online", true);

        mqtt.subscribe(TOPIC_HEIGHT_SET);
        mqtt.subscribe(TOPIC_PRESET_SET);
        mqtt.subscribe(TOPIC_PRESET_SAVE);
        mqtt.subscribe(TOPIC_COMMAND);
        mqtt.subscribe(TOPIC_COLLISION);

        publishDiscovery();

        // Publish current height if known
        if (desk.getLastHeight() > 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%u", desk.getLastHeight());
            mqtt.publish(TOPIC_HEIGHT_STATE, buf, true);
        }
    } else {
        Serial.printf("MQTT: failed rc=%d, retry in %lums\n", mqtt.state(), mqttBackoff);
        mqttBackoff = min(mqttBackoff * 2, MQTT_MAX_BACKOFF);
    }
}

// ── Height publishing (debounced) ───────────────────────────────────

void publishHeight() {
    if (!mqtt.connected()) return;

    uint16_t currentHeight = desk.getLastHeight();
    if (currentHeight == 0) return;
    if (currentHeight == lastPublishedHeight) {
        heightSettled = true;
        return;
    }

    unsigned long now = millis();
    bool moving = desk.isMoving();

    if (moving) {
        // During movement: publish at most every 500ms
        if (now - lastHeightPublishTime < HEIGHT_PUBLISH_INTERVAL) return;
        heightSettled = false;
    } else if (!heightSettled) {
        // Just settled — publish immediately
    } else {
        // Settled and already published — nothing to do
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", currentHeight);
    mqtt.publish(TOPIC_HEIGHT_STATE, buf, true);
    lastPublishedHeight = currentHeight;
    lastHeightPublishTime = now;
    heightSettled = !moving;
}

// ── OTA ─────────────────────────────────────────────────────────────

void setupOta() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }
    ArduinoOTA.onStart([]() { Serial.println("OTA: start"); });
    ArduinoOTA.onEnd([]()   { Serial.println("\nOTA: done"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA: %u%%\r", progress * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA: error %u\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("OTA: ready");
}

void handleOta() {
    ArduinoOTA.handle();
}

// ── Startup queries ─────────────────────────────────────────────────

void handleStartupQueries() {
    if (startupQueriesSent) return;
    if (desk.getState() != DESK_CONNECTED) return;

    // Wait 500ms after connection before querying
    if (deskConnectedTime == 0) {
        deskConnectedTime = millis();
        return;
    }
    if (millis() - deskConnectedTime < 500) return;

    Serial.println("Desk connected — querying settings and limits");
    desk.requestSettings();
    desk.requestPhysicalLimits();
    startupQueriesSent = true;
}

// ── Arduino entry points ────────────────────────────────────────────

void processSimCommand(const String& cmd) {
    if (cmd.startsWith("height ")) {
        long h = cmd.substring(7).toInt();
        if (h > 0 && h < 2000) {
            Serial.printf("SIM: height = %ldmm\n", h);
            desk.simulateHeightReport((uint16_t)h);
        } else {
            Serial.println("SIM: invalid height (use 1-1999)");
        }
    } else if (cmd == "sim on") {
        simMode = true;
        Serial.println("SIM: simulator ON");
    } else if (cmd == "sim off") {
        simMode = false;
        Serial.println("SIM: simulator OFF");
    } else if (cmd == "settings") {
        Serial.println("SIM: injecting settings (limits 620-1270mm)");
        desk.simulateSettingsReport();
    } else if (cmd == "status") {
        Serial.println("──── Status ────");
        Serial.printf("  WiFi:  %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
        if (WiFi.status() == WL_CONNECTED)
            Serial.printf("  IP:    %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  MQTT:  %s\n", mqtt.connected() ? "connected" : "disconnected");
        Serial.printf("  Desk:  %s\n",
            desk.getState() == DESK_CONNECTED ? "connected" :
            desk.getState() == DESK_WAKING ? "waking" : "disconnected");
        Serial.printf("  Height: %umm\n", desk.getLastHeight());
        Serial.printf("  Sim:   %s\n", simMode ? "ON" : "OFF");
        Serial.println("────────────────");
    } else if (cmd == "help") {
        Serial.println("Commands: height <mm>, sim on/off, settings, status, help");
    } else {
        Serial.printf("Unknown command: %s (try 'help')\n", cmd.c_str());
    }
}

void handleSerialInput() {
    static String inputBuf = "";
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (inputBuf.length() > 0) {
                processSimCommand(inputBuf);
                inputBuf = "";
            }
        } else {
            inputBuf += c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== Jarvis Desk Controller — ESP32-C6 ===");
    Serial.println("WiFi + MQTT + Home Assistant");
    Serial.printf("Firmware: %s\n", SW_VERSION);

    // Desk serial
    deskSerial.begin(9600, SERIAL_8N1, DESK_RX_PIN, DESK_TX_PIN);
    desk.onPacket(onDeskPacket);
    desk.onMoveTimeout(onMoveTimeout);
    desk.onTargetReached(onTargetReached);
    desk.begin();

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("WiFi: connecting to %s\n", WIFI_SSID);

    // OTA
    setupOta();
}

void loop() {
    desk.update();
    handleSerialInput();
    handleStartupQueries();
    handleWifi();
    handleMqtt();
    handleOta();
    publishHeight();
}
#endif
