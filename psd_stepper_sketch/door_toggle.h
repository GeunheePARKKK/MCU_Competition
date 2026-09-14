#ifndef DOOR_TOGGLE_H
#define DOOR_TOGGLE_H
#include <stdint.h>
#include "debounce.h"
#ifdef __cplusplus
extern "C" {
#endif
/* TEST ONLY: virtual door state, not a physical closed-door interlock. */
typedef struct {
    debounce_t button;
    uint8_t closed;
    uint8_t press_seen;
} door_toggle_t;
void door_toggle_init(door_toggle_t *door, uint8_t raw, uint32_t filter_ms, uint32_t now);
void door_toggle_update(door_toggle_t *door, uint8_t raw, uint32_t now);
#ifdef __cplusplus
}
#endif
#endif
