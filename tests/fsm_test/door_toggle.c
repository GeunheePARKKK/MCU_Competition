#include "door_toggle.h"
void door_toggle_init(door_toggle_t *door, uint8_t raw, uint32_t filter_ms, uint32_t now) {
    debounce_init(&door->button, raw, DEBOUNCE_LEVEL_HIGH, filter_ms, now);
    door->closed = 0U;
    door->press_seen = 0U;
}
void door_toggle_update(door_toggle_t *door, uint8_t raw, uint32_t now) {
    debounce_update(&door->button, raw, now);
    if (debounce_take_pressed(&door->button)) door->press_seen = 1U;
    if (debounce_take_released(&door->button)) {
        if (door->press_seen) door->closed ^= 1U;
        door->press_seen = 0U;
    }
}
