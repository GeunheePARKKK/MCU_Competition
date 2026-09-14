#ifndef HALL_DETECTOR_H
#define HALL_DETECTOR_H
#include <stdint.h>
#include "train_config.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint16_t adc, delta;
    uint8_t candidate, stable_marker, last_marker, armed, event, rail_fault;
    uint8_t clearing, railing;
    uint32_t candidate_since, clear_since, rail_since;
} hall_detector_t;
void hall_detector_init(hall_detector_t *h, uint32_t now);
void hall_detector_update(hall_detector_t *h, uint16_t adc,
                          uint8_t expected, uint32_t now);
uint8_t hall_classify(uint16_t delta);
#ifdef __cplusplus
}
#endif
#endif

