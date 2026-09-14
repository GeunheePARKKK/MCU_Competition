#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

void timebase_init(void);
uint32_t timebase_millis(void);
uint8_t timebase_has_elapsed(uint32_t now_ms,
                             uint32_t start_ms,
                             uint32_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMEBASE_H */
