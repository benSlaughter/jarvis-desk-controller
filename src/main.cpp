/**
 * Jarvis Desk Controller — Arduino Uno
 *
 * Communicates with a Jarvis/Jiecang standing desk via the RJ-12 port
 * using the reverse-engineered serial protocol.
 *
 * Wiring (RJ-12 to Arduino Uno):
 *   RJ-12 Pin 2 (GND) → Arduino GND
 *   RJ-12 Pin 3 (DTX) → Arduino Pin 2 (RX) via 1kΩ resistor
 *   RJ-12 Pin 4 (VCC) → Not connected (use USB power for development)
 *   RJ-12 Pin 5 (HTX) → Arduino Pin 3 (TX) via 1kΩ resistor
 *
 * Serial monitor: 115200 baud — type commands to control the desk.
 *
 * Protocol reference: https://github.com/phord/Jarvis
 */

#include <SoftwareSerial.h>
#include "jarvis_desk.h"

// --- Pin definitions ---
#define DESK_RX_PIN 2  // Arduino receives from desk DTX (RJ-12 pin 3)
#define DESK_TX_PIN 3  // Arduino transmits to desk HTX (RJ-12 pin 5)

// inverse_logic = true: the desk uses inverted serial (5V pullups, mark = low)
SoftwareSerial deskSerial(DESK_RX_PIN, DESK_TX_PIN, true);

JarvisDesk desk(deskSerial);

// --- Packet receive callback ---
void onDeskPacket(const JarvisPacket& pkt) {
  bool fromController = (pkt.address == JARVIS_ADDR_CONTROLLER);
  const char* source = fromController ? "DESK" : "HS";
  const char* name = jarvis_command_name(pkt.command, fromController);

  Serial.print(F("["));
  Serial.print(source);
  Serial.print(F("] "));
  Serial.print(name);

  // Print params
  if (pkt.length > 0) {
    Serial.print(F(" ("));
    for (uint8_t i = 0; i < pkt.length; i++) {
      if (i > 0) Serial.print(F(" "));
      if (pkt.params[i] < 0x10) Serial.print(F("0"));
      Serial.print(pkt.params[i], HEX);
    }
    Serial.print(F(")"));
  }

  // Decode height for height reports
  if (fromController && pkt.command == RESP_HEIGHT) {
    uint16_t h = jarvis_decode_height(pkt);
    Serial.print(F("  height="));
    Serial.print(h);
  }

  Serial.println();
}

// --- Serial monitor command processing ---
String inputBuffer = "";

void processCommand(const String& cmd) {
  String c = cmd;
  c.trim();
  c.toLowerCase();

  if (c == "up" || c == "raise") {
    Serial.println(F("> Raising desk"));
    desk.raise();
  }
  else if (c == "down" || c == "lower") {
    Serial.println(F("> Lowering desk"));
    desk.lower();
  }
  else if (c == "1") {
    Serial.println(F("> Moving to preset 1"));
    desk.moveToPreset(1);
  }
  else if (c == "2") {
    Serial.println(F("> Moving to preset 2"));
    desk.moveToPreset(2);
  }
  else if (c == "3") {
    Serial.println(F("> Moving to preset 3"));
    desk.moveToPreset(3);
  }
  else if (c == "4") {
    Serial.println(F("> Moving to preset 4"));
    desk.moveToPreset(4);
  }
  else if (c == "save1") {
    Serial.println(F("> Saving preset 1"));
    desk.savePreset(1);
  }
  else if (c == "save2") {
    Serial.println(F("> Saving preset 2"));
    desk.savePreset(2);
  }
  else if (c == "save3") {
    Serial.println(F("> Saving preset 3"));
    desk.savePreset(3);
  }
  else if (c == "save4") {
    Serial.println(F("> Saving preset 4"));
    desk.savePreset(4);
  }
  else if (c == "settings") {
    Serial.println(F("> Requesting settings"));
    desk.requestSettings();
  }
  else if (c == "height") {
    Serial.println(F("> Requesting height"));
    desk.requestHeight();
  }
  else if (c == "limits") {
    Serial.println(F("> Requesting limits"));
    desk.requestLimits();
  }
  else if (c == "wake") {
    Serial.println(F("> Sending wake"));
    desk.sendWake();
  }
  else if (c == "cm") {
    Serial.println(F("> Setting units to cm"));
    desk.setUnits(UNITS_CM);
  }
  else if (c == "in" || c == "inches") {
    Serial.println(F("> Setting units to inches"));
    desk.setUnits(UNITS_IN);
  }
  else if (c == "status") {
    Serial.print(F("> State: "));
    switch (desk.getState()) {
      case DESK_DISCONNECTED: Serial.println(F("DISCONNECTED")); break;
      case DESK_WAKING:       Serial.println(F("WAKING")); break;
      case DESK_CONNECTED:    Serial.println(F("CONNECTED")); break;
    }
    Serial.print(F("> Last height: "));
    Serial.println(desk.getLastHeight());
    Serial.print(F("> Moving: "));
    Serial.println(desk.isMoving() ? F("yes") : F("no"));
  }
  else if (c == "help" || c == "?") {
    Serial.println(F("Commands:"));
    Serial.println(F("  up/raise    - Raise desk one step"));
    Serial.println(F("  down/lower  - Lower desk one step"));
    Serial.println(F("  1/2/3/4     - Move to preset"));
    Serial.println(F("  save1..4    - Save current height to preset"));
    Serial.println(F("  height      - Request current height"));
    Serial.println(F("  settings    - Request all settings"));
    Serial.println(F("  limits      - Request limit settings"));
    Serial.println(F("  wake        - Send wake signal"));
    Serial.println(F("  cm / in     - Set display units"));
    Serial.println(F("  status      - Show connection state"));
    Serial.println(F("  help        - Show this message"));
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(c);
    Serial.println(F("Type 'help' for available commands"));
  }
}

// --- Arduino entry points ---

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F("=== Jarvis Desk Controller ==="));
  Serial.println(F("Type 'help' for commands"));
  Serial.println();

  desk.onPacket(onDeskPacket);
  desk.begin();

  Serial.println(F("Waking desk..."));
}

void loop() {
  // Process desk communication
  desk.update();

  // Process serial monitor input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}
