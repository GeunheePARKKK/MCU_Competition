#include "debounce.h"

#include <stddef.h>

static uint8_t normalize_level(uint8_t level)
{
    return (level != 0U) ? DEBOUNCE_LEVEL_HIGH : DEBOUNCE_LEVEL_LOW;
}

void debounce_init(debounce_t *input,
                   uint8_t initial_raw_level,
                   uint8_t active_level,
                   uint32_t debounce_ms,
                   uint32_t now_ms)
{
    uint8_t initial;

    if (input == NULL)
    {
        return;
    }

    initial = normalize_level(initial_raw_level);

    input->stable_level = initial;
    input->candidate_level = initial;
    input->active_level = normalize_level(active_level);
    input->pressed_event = 0U;
    input->released_event = 0U;
    input->candidate_since_ms = now_ms;
    input->debounce_ms = debounce_ms;
}

void debounce_update(debounce_t *input,
                     uint8_t raw_level,
                     uint32_t now_ms)
{
    uint8_t raw;

    if (input == NULL)
    {
        return;
    }

    raw = normalize_level(raw_level);

    if (raw != input->candidate_level)
    {
        input->candidate_level = raw;
        input->candidate_since_ms = now_ms;
        return;
    }

    if (input->stable_level == input->candidate_level)
    {
        return;
    }

    if ((uint32_t)(now_ms - input->candidate_since_ms) >= input->debounce_ms)
    {
        input->stable_level = input->candidate_level;

        if (input->stable_level == input->active_level)
        {
            input->pressed_event = 1U;
        }
        else
        {
            input->released_event = 1U;
        }
    }
}

uint8_t debounce_is_active(const debounce_t *input)
{
    if (input == NULL)
    {
        return 0U;
    }

    return (uint8_t)(input->stable_level == input->active_level);
}

uint8_t debounce_get_stable_level(const debounce_t *input)
{
    return (input != NULL) ? input->stable_level : DEBOUNCE_LEVEL_LOW;
}

uint8_t debounce_take_pressed(debounce_t *input)
{
    uint8_t event;

    if (input == NULL)
    {
        return 0U;
    }

    event = input->pressed_event;
    input->pressed_event = 0U;
    return event;
}

uint8_t debounce_take_released(debounce_t *input)
{
    uint8_t event;

    if (input == NULL)
    {
        return 0U;
    }

    event = input->released_event;
    input->released_event = 0U;
    return event;
}
