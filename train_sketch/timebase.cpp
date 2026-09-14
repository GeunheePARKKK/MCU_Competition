#include <Arduino.h>
#include "timebase.h"
/* Keep Arduino Timer0 intact; LCD and radio share the Arduino clock. */
void timebase_init(void) {}
uint32_t timebase_millis(void) { return millis(); }
uint8_t timebase_has_elapsed(uint32_t now, uint32_t since, uint32_t interval) {
    return (uint32_t)(now - since) >= interval;
}

