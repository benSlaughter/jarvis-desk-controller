#include "jarvis_desk.h"

JarvisDesk::JarvisDesk(Stream* serial)
  : _serial(serial),
    _callback(nullptr),
    _debugCallback(nullptr),
    _debugEnabled(false),
    _parserState(WAIT_ADDR1),
    _rxAddress(0),
    _rxCommand(0),
    _rxLength(0),
    _rxParamIdx(0),
    _deskState(DESK_DISCONNECTED),
    _lastWakeAttempt(0),
    _lastRxTime(0),
    _wakeRetries(0),
    _lastHeight(0),
    _prevHeight(0),
    _lastHeightTime(0),
    _moving(false),
    _physicalMin(0),
    _physicalMax(0),
    _hasPhysicalLimits(false),
    _moveMode(MOVE_NONE),
    _moveStartTime(0),
    _lastMoveCmdTime(0),
    _timeoutCallback(nullptr),
    _moveToActive(false),
    _moveToTarget(0),
    _moveToStartTime(0),
    _targetReachedCallback(nullptr) {}

void JarvisDesk::setSerial(Stream* serial) {
  _serial = serial;
}

void JarvisDesk::begin() {
  // Caller must configure the serial port (baud rate, polarity, etc.)
  // before calling begin().
  _deskState = DESK_WAKING;
  _lastWakeAttempt = millis();
  _wakeRetries = 0;
}

void JarvisDesk::onPacket(PacketCallback cb) {
  _callback = cb;
}

void JarvisDesk::setDebug(bool enabled) {
  _debugEnabled = enabled;
}

bool JarvisDesk::getDebug() const {
  return _debugEnabled;
}

void JarvisDesk::onDebugByte(DebugCallback cb) {
  _debugCallback = cb;
}

void JarvisDesk::update() {
  // Read all available bytes from desk
  while (_serial->available()) {
    uint8_t b = _serial->read();
    _lastRxTime = millis();
    if (_debugEnabled && _debugCallback) {
      _debugCallback(b);
    }
    processByte(b);
  }

  updateConnectionState();
  updateMoveToHeight();
  updateContinuousMove();
}

// --- Command senders ---

void JarvisDesk::raiseStep() {
  sendCommand(CMD_RAISE);
}

void JarvisDesk::lowerStep() {
  sendCommand(CMD_LOWER);
}

void JarvisDesk::startRaise() {
  _moveMode = MOVE_RAISE;
  _moveStartTime = millis();
  _lastMoveCmdTime = 0;
  raiseStep();  // send first command immediately
}

void JarvisDesk::startLower() {
  _moveMode = MOVE_LOWER;
  _moveStartTime = millis();
  _lastMoveCmdTime = 0;
  lowerStep();  // send first command immediately
}

void JarvisDesk::stop() {
  sendCommand(CMD_STOP);
  _moveMode = MOVE_NONE;
  _moveToActive = false;
}

void JarvisDesk::onMoveTimeout(TimeoutCallback cb) {
  _timeoutCallback = cb;
}

MovementMode JarvisDesk::getMoveMode() const {
  return _moveMode;
}

// --- Move-to-height ---

void JarvisDesk::gotoHeight(uint16_t targetMm) {
  uint8_t params[2] = { (uint8_t)(targetMm >> 8), (uint8_t)(targetMm & 0xFF) };
  sendRawPacket(CMD_GOTO_HEIGHT, 2, params);
}

void JarvisDesk::moveToHeight(uint16_t target) {
  _moveToTarget = target;
  _moveToActive = true;
  _moveToStartTime = millis();

  // Use native goto — desk handles movement and auto-stops
  gotoHeight(target);
}

bool JarvisDesk::isMovingToHeight() const {
  return _moveToActive;
}

uint16_t JarvisDesk::getTargetHeight() const {
  return _moveToTarget;
}

void JarvisDesk::onTargetReached(TargetReachedCallback cb) {
  _targetReachedCallback = cb;
}

void JarvisDesk::moveToPreset(uint8_t preset) {
  switch (preset) {
    case 1: sendCommand(CMD_MOVE_1); break;
    case 2: sendCommand(CMD_MOVE_2); break;
    case 3: sendCommand(CMD_MOVE_3); break;
    case 4: sendCommand(CMD_MOVE_4); break;
  }
}

void JarvisDesk::savePreset(uint8_t preset) {
  switch (preset) {
    case 1: sendCommand(CMD_PROGMEM_1); break;
    case 2: sendCommand(CMD_PROGMEM_2); break;
    case 3: sendCommand(CMD_PROGMEM_3); break;
    case 4: sendCommand(CMD_PROGMEM_4); break;
  }
}

void JarvisDesk::requestSettings() {
  sendCommand(CMD_SETTINGS);
}

void JarvisDesk::requestHeight() {
  // Request settings — the desk responds with height among other data.
  // CMD_WAKE (0x29) doesn't work on all controller models.
  sendCommand(CMD_SETTINGS);
}

void JarvisDesk::requestLimits() {
  sendCommand(CMD_LIMITS);
}

void JarvisDesk::requestPhysicalLimits() {
  sendCommand(CMD_PHYS_LIMITS);
}

void JarvisDesk::setUnits(uint8_t units) {
  sendCommandWithParam(CMD_UNITS, units);
}

void JarvisDesk::setCollisionSensitivity(uint8_t level) {
  sendCommandWithParam(CMD_COLL_SENS, level);
}

void JarvisDesk::setMemoryMode(uint8_t mode) {
  sendCommandWithParam(CMD_MEM_MODE, mode);
}

void JarvisDesk::sendWake() {
  sendCommand(CMD_WAKE);
}

// --- State getters ---

DeskState JarvisDesk::getState() const {
  return _deskState;
}

uint16_t JarvisDesk::getLastHeight() const {
  return _lastHeight;
}

bool JarvisDesk::isMoving() const {
  return _moving;
}

uint16_t JarvisDesk::getPhysicalMin() const {
  return _physicalMin;
}

uint16_t JarvisDesk::getPhysicalMax() const {
  return _physicalMax;
}

bool JarvisDesk::hasPhysicalLimits() const {
  return _hasPhysicalLimits;
}

// --- Internal: continuous movement ---

void JarvisDesk::updateMoveToHeight() {
  if (!_moveToActive) return;

  unsigned long now = millis();

  // Use the later of move start time or last height report as reference
  unsigned long refTime = (_lastHeightTime > _moveToStartTime)
                          ? _lastHeightTime : _moveToStartTime;

  // Safety: no height reports for 2 seconds
  if (now - refTime >= MOVE_TO_HEIGHT_TIMEOUT_MS) {
    _moveToActive = false;
    _moveMode = MOVE_NONE;
    if (_targetReachedCallback) {
      _targetReachedCallback(_moveToTarget, false);
    }
    return;
  }

  // Only evaluate position if we've received a height report since starting
  if (_lastHeightTime <= _moveToStartTime) return;

  int16_t diff = (int16_t)_lastHeight - (int16_t)_moveToTarget;
  int16_t absDiff = (diff < 0) ? -diff : diff;

  // Within tolerance — target reached (desk auto-stops with native goto)
  if (absDiff <= (int16_t)MOVE_TO_TOLERANCE) {
    _moveToActive = false;
    _moveMode = MOVE_NONE;
    if (_targetReachedCallback) {
      _targetReachedCallback(_moveToTarget, true);
    }
    return;
  }
}

void JarvisDesk::updateContinuousMove() {
  if (_moveMode == MOVE_NONE) return;

  unsigned long now = millis();

  // Safety timeout
  if (now - _moveStartTime >= MOVE_TIMEOUT_MS) {
    _moveMode = MOVE_NONE;
    if (_timeoutCallback) {
      _timeoutCallback();
    }
    return;
  }

  // Send repeated commands at MOVE_REPEAT_MS interval
  if (now - _lastMoveCmdTime >= MOVE_REPEAT_MS) {
    _lastMoveCmdTime = now;
    if (_moveMode == MOVE_RAISE) {
      raiseStep();
    } else if (_moveMode == MOVE_LOWER) {
      lowerStep();
    }
  }
}

// --- Internal: send helpers ---

void JarvisDesk::sendCommand(uint8_t command) {
  sendRawPacket(command, 0, nullptr);
}

void JarvisDesk::sendCommandWithParam(uint8_t command, uint8_t param) {
  sendRawPacket(command, 1, &param);
}

void JarvisDesk::sendRawPacket(uint8_t command, uint8_t paramCount, const uint8_t* params) {
  uint8_t buffer[JARVIS_MAX_PACKET_SIZE];
  uint8_t len = jarvis_build_packet(buffer, JARVIS_ADDR_HANDSET, command, paramCount, params);
  _serial->write(buffer, len);
}

// --- Internal: parser state machine ---

void JarvisDesk::resetParser() {
  _parserState = WAIT_ADDR1;
  _rxParamIdx = 0;
}

void JarvisDesk::processByte(uint8_t b) {
  switch (_parserState) {

    case WAIT_ADDR1:
      if (b == JARVIS_ADDR_CONTROLLER || b == JARVIS_ADDR_HANDSET) {
        _rxAddress = b;
        _parserState = WAIT_ADDR2;
      }
      // else: discard, stay in WAIT_ADDR1 (resync)
      break;

    case WAIT_ADDR2:
      if (b == _rxAddress) {
        _parserState = WAIT_CMD;
      } else {
        // Not a valid address pair — resync
        resetParser();
        processByte(b); // re-evaluate this byte as a potential ADDR1
      }
      break;

    case WAIT_CMD:
      _rxCommand = b;
      _parserState = WAIT_LEN;
      break;

    case WAIT_LEN:
      _rxLength = b;
      _rxParamIdx = 0;
      if (_rxLength > JARVIS_MAX_PARAMS) {
        // Invalid length — resync
        resetParser();
      } else if (_rxLength == 0) {
        _parserState = WAIT_CHECKSUM;
      } else {
        _parserState = WAIT_PARAMS;
      }
      break;

    case WAIT_PARAMS:
      _rxParams[_rxParamIdx++] = b;
      if (_rxParamIdx >= _rxLength) {
        _parserState = WAIT_CHECKSUM;
      }
      break;

    case WAIT_CHECKSUM: {
      uint8_t expected = jarvis_checksum(_rxCommand, _rxLength, _rxParams);
      if (b == expected) {
        _parserState = WAIT_EOM;
      } else {
        // Bad checksum — resync
        resetParser();
      }
      break;
    }

    case WAIT_EOM:
      if (b == JARVIS_EOM) {
        // Complete valid packet
        JarvisPacket pkt;
        pkt.address = _rxAddress;
        pkt.command = _rxCommand;
        pkt.length = _rxLength;
        memcpy(pkt.params, _rxParams, _rxLength);
        pkt.checksum = jarvis_checksum(_rxCommand, _rxLength, _rxParams);
        pkt.valid = true;
        handlePacket(pkt);
      }
      // Either way, reset for next packet
      resetParser();
      break;
  }
}

// --- Internal: handle decoded packets ---

void JarvisDesk::handlePacket(const JarvisPacket& pkt) {
  // Track desk state from responses
  if (pkt.address == JARVIS_ADDR_CONTROLLER) {
    if (pkt.command == RESP_HEIGHT && pkt.length >= 2) {
      _prevHeight = _lastHeight;
      _lastHeight = jarvis_decode_height(pkt);
      _lastHeightTime = millis();
      _moving = (_lastHeight != _prevHeight);
    }

    if (pkt.command == RESP_GOTO_HEIGHT && pkt.length >= 2) {
      // Desk echoes target height — just acknowledge
    }

    if (pkt.command == RESP_PHYS_LIMITS && pkt.length >= 4) {
      _physicalMin = ((uint16_t)pkt.params[0] << 8) | pkt.params[1];
      _physicalMax = ((uint16_t)pkt.params[2] << 8) | pkt.params[3];
      _hasPhysicalLimits = true;
    }

    // Any valid response from controller means we're connected
    if (_deskState != DESK_CONNECTED) {
      _deskState = DESK_CONNECTED;
    }
  }

  // Forward to user callback
  if (_callback) {
    _callback(pkt);
  }
}

// --- Internal: connection state machine ---

void JarvisDesk::updateConnectionState() {
  unsigned long now = millis();

  switch (_deskState) {
    case DESK_DISCONNECTED:
      // Start wake sequence
      _deskState = DESK_WAKING;
      _wakeRetries = 0;
      _lastWakeAttempt = now;
      break;

    case DESK_WAKING:
      if (now - _lastWakeAttempt >= WAKE_INTERVAL_MS) {
        if (_wakeRetries < MAX_WAKE_RETRIES) {
          sendWake();
          _wakeRetries++;
          _lastWakeAttempt = now;
        } else {
          // Give up, wait a bit longer before retrying
          _deskState = DESK_DISCONNECTED;
        }
      }
      break;

    case DESK_CONNECTED:
      // Detect disconnection via rx timeout
      if (_lastRxTime > 0 && (now - _lastRxTime > RX_TIMEOUT_MS)) {
        // No data for a while — may be normal (desk display off)
        // Don't aggressively reconnect, just note it
      }
      // Movement detection: if height hasn't changed recently, stop flag
      if (_moving && (now - _lastHeightTime > 1000)) {
        _moving = false;
      }
      break;
  }
}

// ── Simulation ──────────────────────────────────────────────────────

void JarvisDesk::simulateHeightReport(uint16_t heightMm) {
  _lastHeight = heightMm;
  _lastHeightTime = millis();

  if (_callback) {
    JarvisPacket pkt;
    pkt.address = JARVIS_ADDR_CONTROLLER;
    pkt.command = RESP_HEIGHT;
    pkt.length = 3;
    pkt.params[0] = (heightMm >> 8) & 0xFF;
    pkt.params[1] = heightMm & 0xFF;
    pkt.params[2] = 0x0F;
    pkt.checksum = jarvis_checksum(pkt.command, pkt.length, pkt.params);
    pkt.valid = true;
    _callback(pkt);
  }
}

void JarvisDesk::simulateSettingsReport() {
  // Simulate physical limits: 620mm – 1270mm
  _physicalMin = 620;
  _physicalMax = 1270;
  _hasPhysicalLimits = true;

  if (_callback) {
    JarvisPacket pkt;
    pkt.address = JARVIS_ADDR_CONTROLLER;
    pkt.command = RESP_PHYS_LIMITS;
    pkt.length = 4;
    pkt.params[0] = (_physicalMin >> 8) & 0xFF;
    pkt.params[1] = _physicalMin & 0xFF;
    pkt.params[2] = (_physicalMax >> 8) & 0xFF;
    pkt.params[3] = _physicalMax & 0xFF;
    pkt.checksum = jarvis_checksum(pkt.command, pkt.length, pkt.params);
    pkt.valid = true;
    _callback(pkt);
  }
}
