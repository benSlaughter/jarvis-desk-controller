#ifndef JARVIS_DESK_H
#define JARVIS_DESK_H

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

// --- Movement modes ---
enum MovementMode {
  MOVE_NONE,
  MOVE_RAISE,
  MOVE_LOWER
};

// --- Callback types ---
typedef void (*PacketCallback)(const JarvisPacket& pkt);
typedef void (*TimeoutCallback)();
typedef void (*TargetReachedCallback)(uint16_t targetHeight, bool reached);

// --- Callback type for raw debug bytes ---
typedef void (*DebugCallback)(uint8_t byte);

class JarvisDesk {
public:
  JarvisDesk(Stream* serial);

  void begin();
  void update();  // call from loop()

  // Swap the underlying serial port (for polarity detection)
  void setSerial(Stream* serial);

  // Set callback for decoded packets from the desk
  void onPacket(PacketCallback cb);

  // Debug mode: raw byte inspection
  void setDebug(bool enabled);
  bool getDebug() const;
  void onDebugByte(DebugCallback cb);

  // --- Continuous movement ---
  void startRaise();
  void startLower();
  void stop();
  void onMoveTimeout(TimeoutCallback cb);
  MovementMode getMoveMode() const;

  // --- Single-shot commands ---
  void raiseStep();
  void lowerStep();
  void moveToPreset(uint8_t preset); // 1-4
  void savePreset(uint8_t preset);   // 1-4
  void requestSettings();
  void requestHeight();
  void requestLimits();
  void requestPhysicalLimits();
  void setUnits(uint8_t units);      // UNITS_CM or UNITS_IN
  void setCollisionSensitivity(uint8_t level); // COLL_HIGH, COLL_MEDIUM, COLL_LOW
  void setMemoryMode(uint8_t mode);  // MEM_ONE_TOUCH or MEM_CONSTANT_TOUCH
  void sendWake();

  // --- Native goto ---
  void gotoHeight(uint16_t targetMm);

  // --- Move-to-height ---
  void moveToHeight(uint16_t target);
  bool isMovingToHeight() const;
  uint16_t getTargetHeight() const;
  void onTargetReached(TargetReachedCallback cb);

  // --- State ---
  DeskState getState() const;
  uint16_t getLastHeight() const;
  bool isMoving() const;
  uint16_t getPhysicalMin() const;
  uint16_t getPhysicalMax() const;
  bool hasPhysicalLimits() const;

private:
  Stream* _serial;
  PacketCallback _callback;
  DebugCallback _debugCallback;
  bool _debugEnabled;

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

  // Physical limits
  uint16_t _physicalMin;
  uint16_t _physicalMax;
  bool _hasPhysicalLimits;

  // Continuous movement
  MovementMode _moveMode;
  unsigned long _moveStartTime;
  unsigned long _lastMoveCmdTime;
  TimeoutCallback _timeoutCallback;
  static const unsigned long MOVE_REPEAT_MS = 100;
  static const unsigned long MOVE_TIMEOUT_MS = 30000;

  // Move-to-height
  bool _moveToActive;
  uint16_t _moveToTarget;
  unsigned long _moveToStartTime;
  TargetReachedCallback _targetReachedCallback;
  static const uint16_t MOVE_TO_TOLERANCE = 5;
  static const unsigned long MOVE_TO_HEIGHT_TIMEOUT_MS = 2000;

  void updateContinuousMove();
  void updateMoveToHeight();
  void sendCommand(uint8_t command);
  void sendCommandWithParam(uint8_t command, uint8_t param);
  void sendRawPacket(uint8_t command, uint8_t paramCount, const uint8_t* params);
  void processByte(uint8_t b);
  void handlePacket(const JarvisPacket& pkt);
  void resetParser();
  void updateConnectionState();
};

#endif
