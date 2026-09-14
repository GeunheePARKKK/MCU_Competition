#ifndef DEBOUNCE_H
#define DEBOUNCE_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define DEBOUNCE_LEVEL_LOW  UINT8_C(0)
#define DEBOUNCE_LEVEL_HIGH UINT8_C(1)

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t active_level;
    uint8_t pressed_event;
    uint8_t released_event;
    uint32_t candidate_since_ms;
    uint32_t debounce_ms;
} debounce_t;

void debounce_init(debounce_t *input,
                   uint8_t initial_raw_level,
                   uint8_t active_level,
                   uint32_t debounce_ms,
                   uint32_t now_ms);

void debounce_update(debounce_t *input,
                     uint8_t raw_level,
                     uint32_t now_ms);

uint8_t debounce_is_active(const debounce_t *input);
uint8_t debounce_get_stable_level(const debounce_t *input);
uint8_t debounce_take_pressed(debounce_t *input);
uint8_t debounce_take_released(debounce_t *input);


#ifdef __cplusplus
}
#endif

#endif /* DEBOUNCE_H */
