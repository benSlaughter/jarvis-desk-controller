#ifndef JARVIS_PROTOCOL_H
#define JARVIS_PROTOCOL_H

#include <Arduino.h>

// --- Address bytes ---
#define JARVIS_ADDR_HANDSET  0xF1
#define JARVIS_ADDR_CONTROLLER 0xF2

// --- End-of-message marker ---
#define JARVIS_EOM 0x7E

// --- Max payload length ---
#define JARVIS_MAX_PARAMS 4
#define JARVIS_MAX_PACKET_SIZE (2 + 1 + 1 + JARVIS_MAX_PARAMS + 1 + 1) // addr*2 + cmd + len + params + chk + eom

// --- Handset Commands (sent TO the desk controller) ---
#define CMD_RAISE       0x01
#define CMD_LOWER       0x02
#define CMD_PROGMEM_1   0x03
#define CMD_PROGMEM_2   0x04
#define CMD_MOVE_1      0x05
#define CMD_MOVE_2      0x06
#define CMD_SETTINGS    0x07
#define CMD_UNITS       0x0E
#define CMD_MEM_MODE    0x19
#define CMD_COLL_SENS   0x1D
#define CMD_LIMITS      0x20
#define CMD_SET_MAX     0x21
#define CMD_SET_MIN     0x22
#define CMD_LIMIT_CLR   0x23
#define CMD_PROGMEM_3   0x25
#define CMD_PROGMEM_4   0x26
#define CMD_MOVE_3      0x27
#define CMD_MOVE_4      0x28
#define CMD_WAKE        0x29
#define CMD_CALIBRATE   0x91

// --- Desk Responses (sent FROM the desk controller) ---
#define RESP_HEIGHT     0x01
#define RESP_UNITS      0x0E
#define RESP_MEM_MODE   0x19
#define RESP_COLL_SENS  0x1D
#define RESP_LIMITS     0x20
#define RESP_SET_MAX    0x21
#define RESP_SET_MIN    0x22
#define RESP_POSITION_1 0x25
#define RESP_POSITION_2 0x26
#define RESP_POSITION_3 0x27
#define RESP_POSITION_4 0x28
#define RESP_RESET      0x40

// --- Units ---
#define UNITS_CM 0x00
#define UNITS_IN 0x01

// --- Anti-collision sensitivity ---
#define COLL_HIGH   0x01
#define COLL_MEDIUM 0x02
#define COLL_LOW    0x03

// --- Memory mode ---
#define MEM_ONE_TOUCH     0x00
#define MEM_CONSTANT_TOUCH 0x01

// --- Parsed packet structure ---
struct JarvisPacket {
  uint8_t address;
  uint8_t command;
  uint8_t length;
  uint8_t params[JARVIS_MAX_PARAMS];
  uint8_t checksum;
  bool valid;
};

// --- Protocol helper functions ---
uint8_t jarvis_checksum(uint8_t command, uint8_t length, const uint8_t* params);
uint8_t jarvis_build_packet(uint8_t* buffer, uint8_t address, uint8_t command,
                            uint8_t paramCount, const uint8_t* params);
const char* jarvis_command_name(uint8_t command, bool fromController);
uint16_t jarvis_decode_height(const JarvisPacket& pkt);

#endif
