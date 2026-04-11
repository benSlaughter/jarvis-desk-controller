#ifndef ARDUINO_H_MOCK
#define ARDUINO_H_MOCK

#include <stdint.h>
#include <stddef.h>
#include <string.h>

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
