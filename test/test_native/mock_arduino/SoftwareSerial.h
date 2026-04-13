#ifndef SOFTWARE_SERIAL_H_MOCK
#define SOFTWARE_SERIAL_H_MOCK

#include <stdint.h>
#include <stddef.h>

// Minimal Stream base class for native tests
#ifndef STREAM_H_MOCK
#define STREAM_H_MOCK
class Stream {
public:
    virtual ~Stream() {}
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(const uint8_t* buf, size_t len) = 0;
    virtual size_t write(uint8_t b) = 0;
};
#endif

class SoftwareSerial : public Stream {
public:
    SoftwareSerial(int rxPin, int txPin, bool inverse = false)
      : _bufPos(0), _bufLen(0) {
        (void)rxPin; (void)txPin; (void)inverse;
    }

    void begin(long) {}
    void listen() {}

    int available() override {
        return (int)(_bufLen - _bufPos);
    }

    int read() override {
        if (_bufPos < _bufLen) return _buf[_bufPos++];
        return -1;
    }

    size_t write(const uint8_t*, size_t len) override { return len; }
    size_t write(uint8_t) override { return 1; }

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
