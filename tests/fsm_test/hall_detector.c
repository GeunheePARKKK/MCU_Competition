#include "hall_detector.h"
#include <string.h>
uint8_t hall_classify(uint16_t delta) {
    static const uint16_t center[] = {0, 20, 46};
    uint8_t i;
    if (delta <= TRAIN_HALL_CLEAR_DELTA) return 0;
    for (i = 1; i <= 2; ++i)
        if (delta >= center[i] - TRAIN_HALL_TOLERANCE &&
            delta <= center[i] + TRAIN_HALL_TOLERANCE) return i;
    if (delta >= TRAIN_HALL_STOP_DELTA_MIN &&
        delta <= TRAIN_HALL_STOP_DELTA_MAX) return 3;
    return 255;
}
void hall_detector_init(hall_detector_t *h, uint32_t now) {
    memset(h, 0, sizeof(*h));
    h->adc = TRAIN_HALL_BASELINE;
    h->armed = 1;
    h->candidate = h->stable_marker = 255;
    h->candidate_since = now;
}
void hall_detector_update(hall_detector_t *h, uint16_t adc,
                          uint8_t expected, uint32_t now) {
    uint8_t marker;
    h->event = 0;
    h->adc = adc;
    h->delta = adc >= TRAIN_HALL_BASELINE ? adc - TRAIN_HALL_BASELINE
                                         : TRAIN_HALL_BASELINE - adc;
    marker = hall_classify(h->delta);
    if (marker != h->candidate) {
        h->candidate = marker;
        h->candidate_since = now;
        h->stable_marker = 255;
    } else if ((uint32_t)(now - h->candidate_since) >= TRAIN_HALL_STABLE_TIME_MS) {
        h->stable_marker = marker;
    }
    if (h->delta <= TRAIN_HALL_CLEAR_DELTA) {
        if (!h->clearing) { h->clearing = 1; h->clear_since = now; }
        if ((uint32_t)(now - h->clear_since) >= TRAIN_MARKER_REARM_TIME_MS)
            h->armed = 1;
    } else h->clearing = 0;
    if (adc <= 5 || adc >= 1018) {
        if (!h->railing) { h->railing = 1; h->rail_since = now; }
        h->rail_fault = (uint32_t)(now - h->rail_since) >= TRAIN_HALL_RAIL_TIME_MS;
    } else { h->railing = 0; h->rail_fault = 0; }
    if (expected && h->armed && h->stable_marker == expected) {
        h->event = expected;
        h->last_marker = expected;
        h->armed = 0;
    }
}
