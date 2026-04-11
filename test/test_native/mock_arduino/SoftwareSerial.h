#ifndef SOFTWARE_SERIAL_H_MOCK
#define SOFTWARE_SERIAL_H_MOCK

#include <stdint.h>
#include <stddef.h>

class SoftwareSerial {
public:
    SoftwareSerial(int rxPin, int txPin, bool inverse = false)
      : _bufPos(0), _bufLen(0) {
        (void)rxPin; (void)txPin; (void)inverse;
    }

    void begin(long) {}

    int available() {
        return (int)(_bufLen - _bufPos);
    }

    int read() {
        if (_bufPos < _bufLen) return _buf[_bufPos++];
        return -1;
    }

    size_t write(const uint8_t*, size_t len) { return len; }
    size_t write(uint8_t) { return 1; }

    // --- Test helpers ---
    void pushBytes(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len && _bufLen < sizeof(_buf); i++) {
            _buf[_bufLen++] = data[i];
        }
    }

    void resetBuffer() {
        _bufPos = 0;
        _bufLen = 0;
    }

private:
    uint8_t _buf[512];
    size_t _bufPos;
    size_t _bufLen;
};

#endif
