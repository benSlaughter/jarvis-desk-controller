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

  // Decode height for HEIGHT responses (already in real-world mm)
  // Position presets use raw encoder values — don't convert to cm
  if (fromController && pkt.length >= 2) {
    uint16_t h = ((uint16_t)pkt.params[0] << 8) | pkt.params[1];
    switch (pkt.command) {
      case RESP_HEIGHT:
        Serial.print(F("  "));
        Serial.print(h / 10);
        Serial.print('.');
        Serial.print(h % 10);
        Serial.print(F("cm"));
        break;
      case RESP_GOTO_HEIGHT:
        Serial.print(F("  target="));
        Serial.print(h / 10);
        Serial.print('.');
        Serial.print(h % 10);
        Serial.print(F("cm"));
        break;
      case RESP_POSITION_1:
      case RESP_POSITION_2:
      case RESP_POSITION_3:
      case RESP_POSITION_4:
        Serial.print(F("  raw="));
        Serial.print(h);
        break;
      case RESP_SET_MAX:
      case RESP_SET_MIN:
        Serial.print(F("  = "));
        Serial.print(h);
        break;
    }
  }

  // Decode physical limits (4-byte response)
  if (fromController && pkt.command == RESP_PHYS_LIMITS && pkt.length >= 4) {
    uint16_t pmin = ((uint16_t)pkt.params[0] << 8) | pkt.params[1];
    uint16_t pmax = ((uint16_t)pkt.params[2] << 8) | pkt.params[3];
    Serial.print(F("  min="));
    Serial.print(pmin);
    Serial.print(F("mm max="));
    Serial.print(pmax);
    Serial.print(F("mm"));
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
    Serial.println(F("> Sending STOP to desk and clearing internal state"));
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
      Serial.print(F("> Native goto "));
      Serial.print(target);
      Serial.print(F("mm ("));
      Serial.print(target / 10);
      Serial.print('.');
      Serial.print(target % 10);
      Serial.print(F("cm) from "));
      Serial.print(desk.getLastHeight());
      Serial.println(F("mm"));
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
  else if (c == "physlimits") {
    Serial.println(F("> Requesting physical limits"));
    desk.requestPhysicalLimits();
  }
  else if (c == "wake") {
    Serial.println(F("> Sending wake"));
    desk.sendWake();
  }
  else if (c == "units cm") {
    Serial.println(F("> Units set to CM (may not work over RJ-12)"));
    desk.setUnits(UNITS_CM);
  }
  else if (c == "units in") {
    Serial.println(F("> Units set to IN (may not work over RJ-12)"));
    desk.setUnits(UNITS_IN);
  }
  else if (c == "collision high") {
    Serial.println(F("> Anti-collision set to HIGH"));
    desk.setCollisionSensitivity(COLL_HIGH);
  }
  else if (c == "collision medium") {
    Serial.println(F("> Anti-collision set to MEDIUM"));
    desk.setCollisionSensitivity(COLL_MEDIUM);
  }
  else if (c == "collision low") {
    Serial.println(F("> Anti-collision set to LOW"));
    desk.setCollisionSensitivity(COLL_LOW);
  }
  else if (c == "memmode onetouch") {
    Serial.println(F("> Memory mode set to one-touch"));
    desk.setMemoryMode(MEM_ONE_TOUCH);
  }
  else if (c == "memmode constant") {
    Serial.println(F("> Memory mode set to constant-touch"));
    desk.setMemoryMode(MEM_CONSTANT_TOUCH);
  }
  else if (c == "home") {
    if (!desk.hasPhysicalLimits()) {
      Serial.println(F("! Physical limits not known — run 'physlimits' first"));
    } else {
      uint16_t minH = desk.getPhysicalMin();
      Serial.print(F("> Going to physical minimum: "));
      Serial.print(minH);
      Serial.println(F("mm"));
      desk.moveToHeight(minH);
    }
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
  else if (c == "polarity swap") {
    polarityInverted = !polarityInverted;
    activeDeskSerial = polarityInverted ? &deskSerialInverted : &deskSerialNormal;
    activeDeskSerial->begin(9600);
    activeDeskSerial->listen();
    desk.setSerial(activeDeskSerial);
    Serial.print(F("> Switched to "));
    Serial.print(polarityInverted ? F("inverted") : F("normal"));
    Serial.println(F(" polarity"));
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
    if (desk.hasPhysicalLimits()) {
      Serial.print(F("> Physical limits: "));
      Serial.print(desk.getPhysicalMin());
      Serial.print(F("mm - "));
      Serial.print(desk.getPhysicalMax());
      Serial.println(F("mm"));
    }
  }
  else if (c.startsWith("raw ")) {
    // Send raw command with optional hex params: "raw 0C" or "raw 1B 03 06"
    // Parse command byte
    const char* p = c.c_str() + 4;
    char* end;
    long cmdVal = strtol(p, &end, 16);
    if (cmdVal > 0 && cmdVal <= 0xFF) {
      uint8_t cmd = (uint8_t)cmdVal;
      // Parse optional param bytes
      uint8_t params[JARVIS_MAX_PARAMS];
      uint8_t paramCount = 0;
      p = end;
      while (paramCount < JARVIS_MAX_PARAMS && *p) {
        long pv = strtol(p, &end, 16);
        if (end == p) break;
        params[paramCount++] = (uint8_t)pv;
        p = end;
      }
      Serial.print(F("> Sending raw 0x"));
      if (cmd < 0x10) Serial.print('0');
      Serial.print(cmd, HEX);
      if (paramCount > 0) {
        Serial.print(F(" params:"));
        for (uint8_t i = 0; i < paramCount; i++) {
          Serial.print(' ');
          if (params[i] < 0x10) Serial.print('0');
          Serial.print(params[i], HEX);
        }
      }
      Serial.println();
      uint8_t buffer[JARVIS_MAX_PACKET_SIZE];
      uint8_t len = jarvis_build_packet(buffer, JARVIS_ADDR_HANDSET, cmd, paramCount, params);
      activeDeskSerial->write(buffer, len);
    } else {
      Serial.println(F("Usage: raw <cmd> [params...] (hex, e.g., raw 1B 03 06)"));
    }
  }
  else if (c == "scan") {
    Serial.println(F("=== SCAN: Querying all known read commands ==="));
    // Array of {cmd, name} pairs for all known query commands
    static const uint8_t scanCmds[] = {
      0x07, // SETTINGS (returns POS_1-4 + HEIGHT)
      0x08, // UNK_08 → resp 0x05
      0x09, // UNK_09 → resp 0x06
      0x0C, // PHYS_LIMITS → resp 0x07
      0x1C, // UNK_1C → resp 0x1C
      0x1F, // UNK_1F (lock?) → resp 0x1F
      0x20, // LIMITS → resp 0x20
    };
    static const char* const scanNames[] = {
      "SETTINGS(07)",
      "UNK_08",
      "UNK_09",
      "PHYS_LIMITS(0C)",
      "UNK_1C",
      "UNK_1F/LOCK?(1F)",
      "LIMITS(20)",
    };
    static const uint8_t scanCount = sizeof(scanCmds) / sizeof(scanCmds[0]);

    for (uint8_t i = 0; i < scanCount; i++) {
      Serial.print(F("--- "));
      Serial.print(scanNames[i]);
      Serial.println(F(" ---"));
      uint8_t buffer[JARVIS_MAX_PACKET_SIZE];
      uint8_t len = jarvis_build_packet(buffer, JARVIS_ADDR_HANDSET, scanCmds[i], 0, nullptr);
      activeDeskSerial->write(buffer, len);
      // Wait for response — poll for up to 500ms
      unsigned long start = millis();
      bool gotResponse = false;
      while (millis() - start < 500) {
        desk.update();
        if (millis() - start > 50 && !activeDeskSerial->available() && gotResponse) break;
        if (activeDeskSerial->available()) gotResponse = true;
      }
      if (!gotResponse) {
        Serial.println(F("  (no response)"));
      }
    }
    Serial.println(F("=== SCAN COMPLETE ==="));
  }
  else if (c == "help" || c == "?") {
    Serial.println(F("Commands:"));
    Serial.println(F("  up/raise    - Raise desk (continuous)"));
    Serial.println(F("  down/lower  - Lower desk (continuous)"));
    Serial.println(F("  stop        - Stop movement (sends STOP to desk)"));
    Serial.println(F("  goto <mm>   - Move to height (native goto, desk auto-stops)"));
    Serial.println(F("  1/2/3/4     - Move to preset"));
    Serial.println(F("  save1..4    - Save current height to preset"));
    Serial.println(F("  height      - Request current height"));
    Serial.println(F("  settings    - Request all settings"));
    Serial.println(F("  limits      - Request limit settings"));
    Serial.println(F("  physlimits  - Request physical limits"));
    Serial.println(F("  home        - Go to physical minimum height"));
    Serial.println(F("  wake        - Send wake signal"));
    Serial.println(F("  collision high/medium/low - Anti-collision sensitivity"));
    Serial.println(F("  memmode onetouch/constant - Memory preset mode"));
    Serial.println(F("  units cm/in   - Set display units (may not work over RJ-12)"));
    Serial.println(F("  debug on/off  - Toggle raw hex byte output"));
    Serial.println(F("  polarity      - Show serial polarity"));
    Serial.println(F("  polarity swap - Toggle serial polarity"));
    Serial.println(F("  raw <hex>     - Send raw command (e.g., raw 0C)"));
    Serial.println(F("  scan          - Query all known read commands"));
    Serial.println(F("  status        - Show connection state"));
    Serial.println(F("  help          - Show this message"));
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

  // Try normal first (most Jiecang desks use standard UART polarity)
  Serial.print(F("  Trying normal polarity...   "));
  if (tryPolarity(&deskSerialNormal, 2000)) {
    Serial.println(F("OK"));
    activeDeskSerial = &deskSerialNormal;
    polarityInverted = false;
  } else {
    Serial.println(F("no response"));
    Serial.print(F("  Trying inverted polarity... "));
    if (tryPolarity(&deskSerialInverted, 2000)) {
      Serial.println(F("OK"));
      activeDeskSerial = &deskSerialInverted;
      polarityInverted = true;
    } else {
      Serial.println(F("no response"));
      Serial.println(F("  WARNING: No desk detected — defaulting to normal"));
      Serial.println(F("  Use 'polarity swap' to toggle if needed"));
      activeDeskSerial = &deskSerialNormal;
      polarityInverted = false;
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
