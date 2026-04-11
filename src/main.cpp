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

// Two SoftwareSerial instances — only one active at a time.
// SoftwareSerial's inverse_logic is set at construction and cannot change.
SoftwareSerial deskSerialInverted(DESK_RX_PIN, DESK_TX_PIN, true);   // inverted (5V pullups, mark=low)
SoftwareSerial deskSerialNormal(DESK_RX_PIN, DESK_TX_PIN, false);    // standard polarity

SoftwareSerial* activeDeskSerial = &deskSerialInverted;
bool polarityInverted = true;

JarvisDesk desk(&deskSerialInverted);

// --- Debug hex output ---
static const uint8_t DEBUG_BUF_SIZE = 64;
static uint8_t debugBuf[DEBUG_BUF_SIZE];
static uint8_t debugBufLen = 0;
static unsigned long debugLastByteTime = 0;
static const unsigned long DEBUG_FLUSH_MS = 50;

void onDebugByte(uint8_t b) {
  if (debugBufLen < DEBUG_BUF_SIZE) {
    debugBuf[debugBufLen++] = b;
  }
  debugLastByteTime = millis();
}

void flushDebugLine() {
  if (debugBufLen == 0) return;
  Serial.print(F("RX:"));
  for (uint8_t i = 0; i < debugBufLen; i++) {
    Serial.print(' ');
    if (debugBuf[i] < 0x10) Serial.print('0');
    Serial.print(debugBuf[i], HEX);
  }
  Serial.println();
  debugBufLen = 0;
}

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

// --- Safety timeout callback ---
void onMoveTimeout() {
  Serial.println(F("! SAFETY TIMEOUT — movement stopped after 30s"));
}

// --- Move-to-height callback ---
void onTargetReached(uint16_t height, bool reached) {
  if (reached) {
    Serial.print(F("> Target height reached: "));
    Serial.print(height);
    Serial.println(F("mm"));
  } else {
    Serial.println(F("! Move-to-height stopped: no height reports (safety timeout)"));
  }
}

// --- Serial monitor command processing ---
String inputBuffer = "";

void processCommand(const String& cmd) {
  String c = cmd;
  c.trim();
  c.toLowerCase();

  if (c == "up" || c == "raise") {
    Serial.println(F("> Raising desk (continuous — type 'stop' to halt)"));
    desk.startRaise();
  }
  else if (c == "down" || c == "lower") {
    Serial.println(F("> Lowering desk (continuous — type 'stop' to halt)"));
    desk.startLower();
  }
  else if (c == "stop") {
    if (desk.isMovingToHeight()) {
      Serial.println(F("> Stopping move-to-height"));
    } else {
      Serial.println(F("> Stopping movement"));
    }
    desk.stop();
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
  else if (c.startsWith("goto ")) {
    long val = c.substring(5).toInt();
    if (val <= 0) {
      Serial.println(F("Usage: goto <height> (e.g., goto 1000)"));
    } else {
      uint16_t target = (uint16_t)val;
      Serial.print(F("> Moving to "));
      Serial.print(target);
      Serial.print(F("mm (currently at "));
      Serial.print(desk.getLastHeight());
      Serial.println(F("mm)"));
      desk.moveToHeight(target);
    }
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
  else if (c == "debug on") {
    desk.setDebug(true);
    Serial.println(F("> Debug mode ON — raw hex bytes will be shown"));
  }
  else if (c == "debug off") {
    desk.setDebug(false);
    flushDebugLine();
    Serial.println(F("> Debug mode OFF"));
  }
  else if (c == "polarity") {
    Serial.print(F("> Serial polarity: "));
    Serial.println(polarityInverted ? F("inverted") : F("normal"));
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
    if (desk.isMovingToHeight()) {
      Serial.print(F("> Move-to target: "));
      Serial.println(desk.getTargetHeight());
    }
  }
  else if (c == "help" || c == "?") {
    Serial.println(F("Commands:"));
    Serial.println(F("  up/raise    - Raise desk (continuous)"));
    Serial.println(F("  down/lower  - Lower desk (continuous)"));
    Serial.println(F("  stop        - Stop movement"));
    Serial.println(F("  goto <mm>   - Move to specific height (mm)"));
    Serial.println(F("  1/2/3/4     - Move to preset"));
    Serial.println(F("  save1..4    - Save current height to preset"));
    Serial.println(F("  height      - Request current height"));
    Serial.println(F("  settings    - Request all settings"));
    Serial.println(F("  limits      - Request limit settings"));
    Serial.println(F("  wake        - Send wake signal"));
    Serial.println(F("  cm / in     - Set display units"));
    Serial.println(F("  debug on/off- Toggle raw hex byte output"));
    Serial.println(F("  polarity    - Show detected serial polarity"));
    Serial.println(F("  status      - Show connection state"));
    Serial.println(F("  help        - Show this message"));
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(c);
    Serial.println(F("Type 'help' for available commands"));
  }
}

// --- Polarity auto-detection ---
// Try a given SoftwareSerial instance: send WAKE commands and look for a valid
// address byte (0xF2 = desk controller) within the timeout period.
bool tryPolarity(SoftwareSerial* ss, unsigned long timeoutMs) {
  ss->begin(9600);
  ss->listen();

  uint8_t wakePkt[JARVIS_MAX_PACKET_SIZE];
  uint8_t wakeLen = jarvis_build_packet(wakePkt, JARVIS_ADDR_HANDSET, CMD_WAKE, 0, nullptr);
  unsigned long start = millis();
  unsigned long lastWake = 0;

  while (millis() - start < timeoutMs) {
    if (millis() - lastWake >= 250) {
      ss->write(wakePkt, wakeLen);
      lastWake = millis();
    }
    while (ss->available()) {
      uint8_t b = ss->read();
      if (b == JARVIS_ADDR_CONTROLLER || b == JARVIS_ADDR_HANDSET) {
        return true;
      }
    }
  }
  return false;
}

// --- Arduino entry points ---

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F("=== Jarvis Desk Controller ==="));
  Serial.println(F("Auto-detecting serial polarity..."));

  // Try inverted first (most common for Jarvis desks)
  Serial.print(F("  Trying inverted polarity... "));
  if (tryPolarity(&deskSerialInverted, 2000)) {
    Serial.println(F("OK"));
    activeDeskSerial = &deskSerialInverted;
    polarityInverted = true;
  } else {
    Serial.println(F("no response"));
    Serial.print(F("  Trying normal polarity...   "));
    if (tryPolarity(&deskSerialNormal, 2000)) {
      Serial.println(F("OK"));
      activeDeskSerial = &deskSerialNormal;
      polarityInverted = false;
    } else {
      Serial.println(F("no response"));
      Serial.println(F("  WARNING: No desk detected — defaulting to inverted"));
      activeDeskSerial = &deskSerialInverted;
      polarityInverted = true;
    }
  }

  Serial.print(F("Polarity: "));
  Serial.println(polarityInverted ? F("inverted") : F("normal"));
  Serial.println(F("Type 'help' for commands"));
  Serial.println();

  desk.setSerial(activeDeskSerial);
  desk.onPacket(onDeskPacket);
  desk.onDebugByte(onDebugByte);
  desk.onMoveTimeout(onMoveTimeout);
  desk.onTargetReached(onTargetReached);
  desk.begin();

  Serial.println(F("Waking desk..."));
}

void loop() {
  // Process desk communication
  desk.update();

  // Flush debug line after packet gap
  if (debugBufLen > 0 && (millis() - debugLastByteTime >= DEBUG_FLUSH_MS)) {
    flushDebugLine();
  }

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
