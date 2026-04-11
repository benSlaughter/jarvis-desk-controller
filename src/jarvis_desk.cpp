#include "jarvis_desk.h"

JarvisDesk::JarvisDesk(SoftwareSerial& serial)
  : _serial(serial),
    _callback(nullptr),
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
    _moving(false) {}

void JarvisDesk::begin() {
  // SoftwareSerial with inverse_logic=true is set by the caller.
  // 9600 baud, 8N1
  _serial.begin(9600);
  _deskState = DESK_WAKING;
  _lastWakeAttempt = millis();
  _wakeRetries = 0;
}

void JarvisDesk::onPacket(PacketCallback cb) {
  _callback = cb;
}

void JarvisDesk::update() {
  // Read all available bytes from desk
  while (_serial.available()) {
    uint8_t b = _serial.read();
    _lastRxTime = millis();
    processByte(b);
  }

  updateConnectionState();
}

// --- Command senders ---

void JarvisDesk::raise() {
  sendCommand(CMD_RAISE);
}

void JarvisDesk::lower() {
  sendCommand(CMD_LOWER);
}

void JarvisDesk::stop() {
  // Sending no button / releasing is implicit when we stop sending
  // raise/lower. The desk stops when it stops receiving move commands.
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
  // Wake the desk to trigger height reports
  sendCommand(CMD_WAKE);
}

void JarvisDesk::requestLimits() {
  sendCommand(CMD_LIMITS);
}

void JarvisDesk::setUnits(uint8_t units) {
  sendCommandWithParam(CMD_UNITS, units);
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
  _serial.write(buffer, len);
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
