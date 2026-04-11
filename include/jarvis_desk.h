#ifndef JARVIS_DESK_H
#define JARVIS_DESK_H

#include <SoftwareSerial.h>
#include "jarvis_protocol.h"

// --- Parser states ---
enum ParserState {
  WAIT_ADDR1,
  WAIT_ADDR2,
  WAIT_CMD,
  WAIT_LEN,
  WAIT_PARAMS,
  WAIT_CHECKSUM,
  WAIT_EOM
};

// --- Connection states ---
enum DeskState {
  DESK_DISCONNECTED,
  DESK_WAKING,
  DESK_CONNECTED
};

// --- Callback type for received packets ---
typedef void (*PacketCallback)(const JarvisPacket& pkt);

class JarvisDesk {
public:
  JarvisDesk(SoftwareSerial& serial);

  void begin();
  void update();  // call from loop()

  // Set callback for decoded packets from the desk
  void onPacket(PacketCallback cb);

  // --- Desk commands ---
  void raise();
  void lower();
  void stop();
  void moveToPreset(uint8_t preset); // 1-4
  void savePreset(uint8_t preset);   // 1-4
  void requestSettings();
  void requestHeight();
  void requestLimits();
  void setUnits(uint8_t units);      // UNITS_CM or UNITS_IN
  void sendWake();

  // --- State ---
  DeskState getState() const;
  uint16_t getLastHeight() const;
  bool isMoving() const;

private:
  SoftwareSerial& _serial;
  PacketCallback _callback;

  // Parser
  ParserState _parserState;
  uint8_t _rxAddress;
  uint8_t _rxCommand;
  uint8_t _rxLength;
  uint8_t _rxParams[JARVIS_MAX_PARAMS];
  uint8_t _rxParamIdx;

  // Connection
  DeskState _deskState;
  unsigned long _lastWakeAttempt;
  unsigned long _lastRxTime;
  uint8_t _wakeRetries;
  static const unsigned long WAKE_INTERVAL_MS = 500;
  static const unsigned long RX_TIMEOUT_MS = 3000;
  static const uint8_t MAX_WAKE_RETRIES = 20;

  // Desk state tracking
  uint16_t _lastHeight;
  uint16_t _prevHeight;
  unsigned long _lastHeightTime;
  bool _moving;

  void sendCommand(uint8_t command);
  void sendCommandWithParam(uint8_t command, uint8_t param);
  void sendRawPacket(uint8_t command, uint8_t paramCount, const uint8_t* params);
  void processByte(uint8_t b);
  void handlePacket(const JarvisPacket& pkt);
  void resetParser();
  void updateConnectionState();
};

#endif
