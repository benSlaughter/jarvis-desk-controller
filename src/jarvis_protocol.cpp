#include "jarvis_protocol.h"

uint8_t jarvis_checksum(uint8_t command, uint8_t length, const uint8_t* params) {
  uint8_t sum = command + length;
  for (uint8_t i = 0; i < length; i++) {
    sum += params[i];
  }
  return sum; // natural uint8_t overflow gives us & 0xFF
}

uint8_t jarvis_build_packet(uint8_t* buffer, uint8_t address, uint8_t command,
                            uint8_t paramCount, const uint8_t* params) {
  if (paramCount > JARVIS_MAX_PARAMS) {
    paramCount = JARVIS_MAX_PARAMS;
  }

  uint8_t idx = 0;
  buffer[idx++] = address;
  buffer[idx++] = address;
  buffer[idx++] = command;
  buffer[idx++] = paramCount;

  for (uint8_t i = 0; i < paramCount; i++) {
    buffer[idx++] = params[i];
  }

  buffer[idx++] = jarvis_checksum(command, paramCount, params);
  buffer[idx++] = JARVIS_EOM;

  return idx; // total packet length
}

uint16_t jarvis_decode_height(const JarvisPacket& pkt) {
  if (pkt.command != RESP_HEIGHT || pkt.length < 2) {
    return 0;
  }
  return ((uint16_t)pkt.params[0] << 8) | pkt.params[1];
}

const char* jarvis_command_name(uint8_t command, bool fromController) {
  if (fromController) {
    switch (command) {
      case RESP_HEIGHT:     return "HEIGHT";
      case RESP_UNITS:      return "UNITS";
      case RESP_MEM_MODE:   return "MEM_MODE";
      case RESP_COLL_SENS:  return "COLL_SENS";
      case RESP_LIMITS:     return "LIMITS";
      case RESP_SET_MAX:    return "SET_MAX";
      case RESP_SET_MIN:    return "SET_MIN";
      case RESP_POSITION_1: return "POS_1";
      case RESP_POSITION_2: return "POS_2";
      case RESP_POSITION_3: return "POS_3";
      case RESP_POSITION_4: return "POS_4";
      case RESP_RESET:      return "RESET";
      case 0x05:            return "UNK_05";
      case 0x06:            return "UNK_06";
      case 0x07:            return "UNK_07";
      default:              return "UNKNOWN";
    }
  } else {
    switch (command) {
      case CMD_RAISE:       return "RAISE";
      case CMD_LOWER:       return "LOWER";
      case CMD_PROGMEM_1:   return "PROGMEM_1";
      case CMD_PROGMEM_2:   return "PROGMEM_2";
      case CMD_MOVE_1:      return "MOVE_1";
      case CMD_MOVE_2:      return "MOVE_2";
      case CMD_SETTINGS:    return "SETTINGS";
      case CMD_UNITS:       return "UNITS";
      case CMD_MEM_MODE:    return "MEM_MODE";
      case CMD_COLL_SENS:   return "COLL_SENS";
      case CMD_LIMITS:      return "LIMITS";
      case CMD_SET_MAX:     return "SET_MAX";
      case CMD_SET_MIN:     return "SET_MIN";
      case CMD_LIMIT_CLR:   return "LIMIT_CLR";
      case CMD_PROGMEM_3:   return "PROGMEM_3";
      case CMD_PROGMEM_4:   return "PROGMEM_4";
      case CMD_MOVE_3:      return "MOVE_3";
      case CMD_MOVE_4:      return "MOVE_4";
      case CMD_WAKE:        return "WAKE";
      case CMD_CALIBRATE:   return "CALIBRATE";
      default:              return "UNKNOWN";
    }
  }
}
