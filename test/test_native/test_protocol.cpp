#include <unity.h>
#include "jarvis_protocol.h"
#include "jarvis_desk.h"
#include <SoftwareSerial.h>

// millis() backing store
unsigned long _mock_millis = 0;

// =====================================================================
// jarvis_checksum tests
// =====================================================================

void test_checksum_no_params(void) {
    uint8_t chk = jarvis_checksum(0x01, 0, nullptr);
    // cmd(0x01) + len(0) = 0x01
    TEST_ASSERT_EQUAL_HEX8(0x01, chk);
}

void test_checksum_single_param(void) {
    uint8_t p[] = {0x10};
    uint8_t chk = jarvis_checksum(0x01, 1, p);
    // 0x01 + 0x01 + 0x10 = 0x12
    TEST_ASSERT_EQUAL_HEX8(0x12, chk);
}

void test_checksum_two_params(void) {
    uint8_t p[] = {0x03, 0x04};
    uint8_t chk = jarvis_checksum(0x01, 2, p);
    // 0x01 + 0x02 + 0x03 + 0x04 = 0x0A
    TEST_ASSERT_EQUAL_HEX8(0x0A, chk);
}

void test_checksum_max_params(void) {
    uint8_t p[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chk = jarvis_checksum(0x29, 4, p);
    // 0x29 + 0x04 + 0x01 + 0x02 + 0x03 + 0x04 = 0x37
    TEST_ASSERT_EQUAL_HEX8(0x37, chk);
}

void test_checksum_overflow_wrapping(void) {
    uint8_t p[] = {0xFF};
    uint8_t chk = jarvis_checksum(0xFF, 1, p);
    // 0xFF + 0x01 + 0xFF = 0x1FF => wraps to 0xFF
    TEST_ASSERT_EQUAL_HEX8(0xFF, chk);
}

void test_checksum_overflow_wrapping_to_zero(void) {
    uint8_t p[] = {0xFF};
    uint8_t chk = jarvis_checksum(0x00, 1, p);
    // 0x00 + 0x01 + 0xFF = 0x100 => wraps to 0x00
    TEST_ASSERT_EQUAL_HEX8(0x00, chk);
}

void test_checksum_all_zeros(void) {
    uint8_t p[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t chk = jarvis_checksum(0x00, 4, p);
    // 0x00 + 0x04 + 0 + 0 + 0 + 0 = 0x04
    TEST_ASSERT_EQUAL_HEX8(0x04, chk);
}

// =====================================================================
// jarvis_build_packet tests
// =====================================================================

void test_build_packet_handset_no_params(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_HANDSET, CMD_RAISE, 0, nullptr);

    TEST_ASSERT_EQUAL(6, len);
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[0]); // addr1
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[1]); // addr2
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[2]); // cmd = CMD_RAISE
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]); // len = 0
    // checksum: cmd(0x01) + len(0) = 0x01
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, buf[5]); // EOM
}

void test_build_packet_controller_no_params(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_CONTROLLER, 0x40, 0, nullptr);

    TEST_ASSERT_EQUAL(6, len);
    TEST_ASSERT_EQUAL_HEX8(0xF2, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF2, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x40, buf[2]); // RESP_RESET
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x40, buf[4]); // checksum = 0x40 + 0x00
    TEST_ASSERT_EQUAL_HEX8(0x7E, buf[5]);
}

void test_build_packet_with_one_param(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t p[] = {UNITS_CM};
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_HANDSET, CMD_UNITS, 1, p);

    TEST_ASSERT_EQUAL(7, len);
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0E, buf[2]); // CMD_UNITS
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[3]); // len
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[4]); // param: UNITS_CM
    // checksum: 0x0E + 0x01 + 0x00 = 0x0F
    TEST_ASSERT_EQUAL_HEX8(0x0F, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, buf[6]);
}

void test_build_packet_with_two_params(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t p[] = {0x03, 0x20};
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_CONTROLLER, RESP_HEIGHT, 2, p);

    TEST_ASSERT_EQUAL(8, len);
    TEST_ASSERT_EQUAL_HEX8(0xF2, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF2, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[2]); // RESP_HEIGHT
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[3]); // len = 2
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[4]); // param0
    TEST_ASSERT_EQUAL_HEX8(0x20, buf[5]); // param1
    // checksum: 0x01 + 0x02 + 0x03 + 0x20 = 0x26
    TEST_ASSERT_EQUAL_HEX8(0x26, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, buf[7]);
}

void test_build_packet_clamps_excess_params(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t p[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_HANDSET, CMD_WAKE, 6, p);

    // Should clamp to JARVIS_MAX_PARAMS (4)
    TEST_ASSERT_EQUAL(10, len); // 2+1+1+4+1+1
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[3]); // len clamped to 4
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]); // only first 4 params
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[7]);
}

void test_build_packet_wake_command(void) {
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_HANDSET, CMD_WAKE, 0, nullptr);

    TEST_ASSERT_EQUAL(6, len);
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x29, buf[2]); // CMD_WAKE
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x29, buf[4]); // checksum = 0x29 + 0x00
    TEST_ASSERT_EQUAL_HEX8(0x7E, buf[5]);
}

// =====================================================================
// jarvis_decode_height tests
// =====================================================================

void test_decode_height_normal(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 2;
    pkt.params[0] = 0x03;
    pkt.params[1] = 0x20;
    pkt.valid = true;

    uint16_t h = jarvis_decode_height(pkt);
    // (0x03 << 8) | 0x20 = 0x0320 = 800
    TEST_ASSERT_EQUAL_UINT16(800, h);
}

void test_decode_height_minimum(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 2;
    pkt.params[0] = 0x00;
    pkt.params[1] = 0x01;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(1, jarvis_decode_height(pkt));
}

void test_decode_height_max_value(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 2;
    pkt.params[0] = 0xFF;
    pkt.params[1] = 0xFF;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(0xFFFF, jarvis_decode_height(pkt));
}

void test_decode_height_typical_desk_mm(void) {
    // 720mm = 0x02D0
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 2;
    pkt.params[0] = 0x02;
    pkt.params[1] = 0xD0;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(720, jarvis_decode_height(pkt));
}

void test_decode_height_inches_range(void) {
    // ~28 inches = 0x001C
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 2;
    pkt.params[0] = 0x00;
    pkt.params[1] = 0x1C;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(28, jarvis_decode_height(pkt));
}

void test_decode_height_wrong_command(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_RESET; // 0x40, not RESP_HEIGHT (0x01)
    pkt.length = 2;
    pkt.params[0] = 0x03;
    pkt.params[1] = 0x20;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(0, jarvis_decode_height(pkt));
}

void test_decode_height_too_short(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 1; // need at least 2
    pkt.params[0] = 0x03;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(0, jarvis_decode_height(pkt));
}

void test_decode_height_zero_length(void) {
    JarvisPacket pkt = {};
    pkt.command = RESP_HEIGHT;
    pkt.length = 0;
    pkt.valid = true;

    TEST_ASSERT_EQUAL_UINT16(0, jarvis_decode_height(pkt));
}

// =====================================================================
// jarvis_command_name tests
// =====================================================================

void test_command_name_handset_raise(void) {
    TEST_ASSERT_EQUAL_STRING("RAISE", jarvis_command_name(CMD_RAISE, false));
}

void test_command_name_handset_lower(void) {
    TEST_ASSERT_EQUAL_STRING("LOWER", jarvis_command_name(CMD_LOWER, false));
}

void test_command_name_handset_wake(void) {
    TEST_ASSERT_EQUAL_STRING("WAKE", jarvis_command_name(CMD_WAKE, false));
}

void test_command_name_handset_calibrate(void) {
    TEST_ASSERT_EQUAL_STRING("CALIBRATE", jarvis_command_name(CMD_CALIBRATE, false));
}

void test_command_name_handset_all_moves(void) {
    TEST_ASSERT_EQUAL_STRING("MOVE_1", jarvis_command_name(CMD_MOVE_1, false));
    TEST_ASSERT_EQUAL_STRING("MOVE_2", jarvis_command_name(CMD_MOVE_2, false));
    TEST_ASSERT_EQUAL_STRING("MOVE_3", jarvis_command_name(CMD_MOVE_3, false));
    TEST_ASSERT_EQUAL_STRING("MOVE_4", jarvis_command_name(CMD_MOVE_4, false));
}

void test_command_name_handset_all_progmem(void) {
    TEST_ASSERT_EQUAL_STRING("PROGMEM_1", jarvis_command_name(CMD_PROGMEM_1, false));
    TEST_ASSERT_EQUAL_STRING("PROGMEM_2", jarvis_command_name(CMD_PROGMEM_2, false));
    TEST_ASSERT_EQUAL_STRING("PROGMEM_3", jarvis_command_name(CMD_PROGMEM_3, false));
    TEST_ASSERT_EQUAL_STRING("PROGMEM_4", jarvis_command_name(CMD_PROGMEM_4, false));
}

void test_command_name_handset_unknown(void) {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", jarvis_command_name(0xFE, false));
}

void test_command_name_controller_height(void) {
    TEST_ASSERT_EQUAL_STRING("HEIGHT", jarvis_command_name(RESP_HEIGHT, true));
}

void test_command_name_controller_reset(void) {
    TEST_ASSERT_EQUAL_STRING("RESET", jarvis_command_name(RESP_RESET, true));
}

void test_command_name_controller_positions(void) {
    TEST_ASSERT_EQUAL_STRING("POS_1", jarvis_command_name(RESP_POSITION_1, true));
    TEST_ASSERT_EQUAL_STRING("POS_2", jarvis_command_name(RESP_POSITION_2, true));
    TEST_ASSERT_EQUAL_STRING("POS_3", jarvis_command_name(RESP_POSITION_3, true));
    TEST_ASSERT_EQUAL_STRING("POS_4", jarvis_command_name(RESP_POSITION_4, true));
}

void test_command_name_controller_settings(void) {
    TEST_ASSERT_EQUAL_STRING("UNITS", jarvis_command_name(RESP_UNITS, true));
    TEST_ASSERT_EQUAL_STRING("MEM_MODE", jarvis_command_name(RESP_MEM_MODE, true));
    TEST_ASSERT_EQUAL_STRING("COLL_SENS", jarvis_command_name(RESP_COLL_SENS, true));
    TEST_ASSERT_EQUAL_STRING("LIMITS", jarvis_command_name(RESP_LIMITS, true));
    TEST_ASSERT_EQUAL_STRING("SET_MAX", jarvis_command_name(RESP_SET_MAX, true));
    TEST_ASSERT_EQUAL_STRING("SET_MIN", jarvis_command_name(RESP_SET_MIN, true));
}

void test_command_name_controller_unknown(void) {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", jarvis_command_name(0xFE, true));
}

void test_command_name_controller_unk_codes(void) {
    TEST_ASSERT_EQUAL_STRING("UNK_05", jarvis_command_name(0x05, true));
    TEST_ASSERT_EQUAL_STRING("UNK_06", jarvis_command_name(0x06, true));
    TEST_ASSERT_EQUAL_STRING("PHYS_LIMITS", jarvis_command_name(0x07, true));
}

// =====================================================================
// Parser state machine tests (via JarvisDesk)
// =====================================================================

static JarvisPacket _lastPkt;
static int _pktCount;

static void testCallback(const JarvisPacket& pkt) {
    _lastPkt = pkt;
    _pktCount++;
}

static void resetTestState(void) {
    memset(&_lastPkt, 0, sizeof(_lastPkt));
    _pktCount = 0;
    _mock_millis = 0;
}

void test_parser_valid_height_packet(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Build a valid controller height packet: F2 F2 01 02 03 20 26 7E
    uint8_t pkt[] = {0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E};
    mockSerial.pushBytes(pkt, sizeof(pkt));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(JARVIS_ADDR_CONTROLLER, _lastPkt.address);
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
    TEST_ASSERT_EQUAL(2, _lastPkt.length);
    TEST_ASSERT_EQUAL_HEX8(0x03, _lastPkt.params[0]);
    TEST_ASSERT_EQUAL_HEX8(0x20, _lastPkt.params[1]);
    TEST_ASSERT_TRUE(_lastPkt.valid);
}

void test_parser_valid_no_param_packet(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Handset RAISE: F1 F1 01 00 01 7E
    uint8_t pkt[] = {0xF1, 0xF1, 0x01, 0x00, 0x01, 0x7E};
    mockSerial.pushBytes(pkt, sizeof(pkt));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(JARVIS_ADDR_HANDSET, _lastPkt.address);
    TEST_ASSERT_EQUAL_HEX8(CMD_RAISE, _lastPkt.command);
    TEST_ASSERT_EQUAL(0, _lastPkt.length);
    TEST_ASSERT_TRUE(_lastPkt.valid);
}

void test_parser_two_consecutive_packets(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Two handset packets back-to-back
    uint8_t data[] = {
        0xF1, 0xF1, 0x01, 0x00, 0x01, 0x7E,  // RAISE
        0xF1, 0xF1, 0x02, 0x00, 0x02, 0x7E,  // LOWER
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(2, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(CMD_LOWER, _lastPkt.command); // last received
}

void test_parser_resync_bad_checksum(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Bad packet (wrong checksum), followed by a good packet
    uint8_t data[] = {
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0xFF, 0x7E,  // bad checksum (0xFF != 0x26)
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E,  // good packet
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount); // only good one parsed
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
    TEST_ASSERT_EQUAL_HEX8(0x03, _lastPkt.params[0]);
    TEST_ASSERT_EQUAL_HEX8(0x20, _lastPkt.params[1]);
}

void test_parser_resync_bad_length(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Packet with length > JARVIS_MAX_PARAMS (triggers resync), then a good packet
    uint8_t data[] = {
        0xF2, 0xF2, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E,  // len=5 (invalid)
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E,  // good packet
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
}

void test_parser_noise_before_packet(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Random noise bytes followed by a valid packet
    uint8_t data[] = {
        0x00, 0x55, 0xAA, 0x42, 0x13,             // noise
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E,  // good packet
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
}

void test_parser_mismatched_address_pair(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // F1 followed by F2 (mismatch) — parser should resync
    // Then F2 becomes new ADDR1, next F2 matches, and rest of packet is valid
    uint8_t data[] = {
        0xF1, 0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E,
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(JARVIS_ADDR_CONTROLLER, _lastPkt.address);
}

void test_parser_partial_then_complete(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Feed first half
    uint8_t part1[] = {0xF2, 0xF2, 0x01, 0x02};
    mockSerial.pushBytes(part1, sizeof(part1));
    desk.update();
    TEST_ASSERT_EQUAL(0, _pktCount); // not complete yet

    // Feed second half
    mockSerial.resetBuffer();
    uint8_t part2[] = {0x03, 0x20, 0x26, 0x7E};
    mockSerial.pushBytes(part2, sizeof(part2));
    desk.update();
    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
}

void test_parser_bad_eom_resets(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Valid everything except EOM byte is wrong
    uint8_t data[] = {
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x00,  // EOM=0x00 (wrong)
        0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E,  // good
    };
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    // First packet: EOM is 0x00, not 0x7E — parser resets. Second packet is valid.
    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(RESP_HEIGHT, _lastPkt.command);
}

void test_parser_controller_packet_with_four_params(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    uint8_t p[] = {0x0A, 0x0B, 0x0C, 0x0D};
    uint8_t chk = jarvis_checksum(0x20, 4, p); // RESP_LIMITS
    uint8_t data[] = {0xF2, 0xF2, 0x20, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, chk, 0x7E};
    mockSerial.pushBytes(data, sizeof(data));
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_HEX8(0x20, _lastPkt.command);
    TEST_ASSERT_EQUAL(4, _lastPkt.length);
    TEST_ASSERT_EQUAL_HEX8(0x0A, _lastPkt.params[0]);
    TEST_ASSERT_EQUAL_HEX8(0x0B, _lastPkt.params[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0C, _lastPkt.params[2]);
    TEST_ASSERT_EQUAL_HEX8(0x0D, _lastPkt.params[3]);
}

void test_parser_all_noise_no_packets(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    uint8_t noise[] = {0x00, 0x01, 0x02, 0x55, 0xAA, 0xFF, 0x7E, 0x12, 0x34};
    mockSerial.pushBytes(noise, sizeof(noise));
    desk.update();

    TEST_ASSERT_EQUAL(0, _pktCount);
}

void test_parser_height_tracking(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Send a height packet from controller
    uint8_t pkt[] = {0xF2, 0xF2, 0x01, 0x02, 0x03, 0x20, 0x26, 0x7E};
    mockSerial.pushBytes(pkt, sizeof(pkt));
    desk.update();

    TEST_ASSERT_EQUAL_UINT16(800, desk.getLastHeight());
}

void test_parser_roundtrip_build_then_parse(void) {
    resetTestState();
    SoftwareSerial mockSerial(0, 0);
    JarvisDesk desk(&mockSerial);
    desk.onPacket(testCallback);

    // Build a packet with jarvis_build_packet, then parse it
    uint8_t buf[JARVIS_MAX_PACKET_SIZE];
    uint8_t p[] = {0x02, 0xD0}; // 720mm
    uint8_t len = jarvis_build_packet(buf, JARVIS_ADDR_CONTROLLER, RESP_HEIGHT, 2, p);

    mockSerial.pushBytes(buf, len);
    desk.update();

    TEST_ASSERT_EQUAL(1, _pktCount);
    TEST_ASSERT_EQUAL_UINT16(720, jarvis_decode_height(_lastPkt));
}

// =====================================================================
// Main
// =====================================================================

int main(void) {
    UNITY_BEGIN();

    // Checksum tests
    RUN_TEST(test_checksum_no_params);
    RUN_TEST(test_checksum_single_param);
    RUN_TEST(test_checksum_two_params);
    RUN_TEST(test_checksum_max_params);
    RUN_TEST(test_checksum_overflow_wrapping);
    RUN_TEST(test_checksum_overflow_wrapping_to_zero);
    RUN_TEST(test_checksum_all_zeros);

    // Build packet tests
    RUN_TEST(test_build_packet_handset_no_params);
    RUN_TEST(test_build_packet_controller_no_params);
    RUN_TEST(test_build_packet_with_one_param);
    RUN_TEST(test_build_packet_with_two_params);
    RUN_TEST(test_build_packet_clamps_excess_params);
    RUN_TEST(test_build_packet_wake_command);

    // Decode height tests
    RUN_TEST(test_decode_height_normal);
    RUN_TEST(test_decode_height_minimum);
    RUN_TEST(test_decode_height_max_value);
    RUN_TEST(test_decode_height_typical_desk_mm);
    RUN_TEST(test_decode_height_inches_range);
    RUN_TEST(test_decode_height_wrong_command);
    RUN_TEST(test_decode_height_too_short);
    RUN_TEST(test_decode_height_zero_length);

    // Command name tests
    RUN_TEST(test_command_name_handset_raise);
    RUN_TEST(test_command_name_handset_lower);
    RUN_TEST(test_command_name_handset_wake);
    RUN_TEST(test_command_name_handset_calibrate);
    RUN_TEST(test_command_name_handset_all_moves);
    RUN_TEST(test_command_name_handset_all_progmem);
    RUN_TEST(test_command_name_handset_unknown);
    RUN_TEST(test_command_name_controller_height);
    RUN_TEST(test_command_name_controller_reset);
    RUN_TEST(test_command_name_controller_positions);
    RUN_TEST(test_command_name_controller_settings);
    RUN_TEST(test_command_name_controller_unknown);
    RUN_TEST(test_command_name_controller_unk_codes);

    // Parser state machine tests
    RUN_TEST(test_parser_valid_height_packet);
    RUN_TEST(test_parser_valid_no_param_packet);
    RUN_TEST(test_parser_two_consecutive_packets);
    RUN_TEST(test_parser_resync_bad_checksum);
    RUN_TEST(test_parser_resync_bad_length);
    RUN_TEST(test_parser_noise_before_packet);
    RUN_TEST(test_parser_mismatched_address_pair);
    RUN_TEST(test_parser_partial_then_complete);
    RUN_TEST(test_parser_bad_eom_resets);
    RUN_TEST(test_parser_controller_packet_with_four_params);
    RUN_TEST(test_parser_all_noise_no_packets);
    RUN_TEST(test_parser_height_tracking);
    RUN_TEST(test_parser_roundtrip_build_then_parse);

    return UNITY_END();
}
