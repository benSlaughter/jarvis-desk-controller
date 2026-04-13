#ifndef ARDUINO_H_MOCK
#define ARDUINO_H_MOCK

#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

// millis() stub — returns a controllable value for tests
#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long _mock_millis;
static inline unsigned long millis(void) { return _mock_millis; }

#ifdef __cplusplus
}
#endif

#endif
