#include "hall_detector.h"
#include <string.h>
/* 자석 1개씩 세는 방식: 0 = 자석 없음, 1 = 자석 있음(극·세기 무관), 255 = 경계 구간(보류) */
uint8_t hall_classify(uint16_t delta) {
    if (delta <= TRAIN_HALL_CLEAR_DELTA) return 0;
    if (delta >= TRAIN_HALL_PRESENT_DELTA) return 1;
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
    /*
     * 자석이 없던 구간(150ms 이상)을 지난 뒤 새 자석을 만나면 한 번만 이벤트를 낸다.
     * 이벤트 번호는 FSM이 기다리던 순서다: 2 = 출발 후 첫 번째 자석(감속), 3 = 두 번째(정차).
     * 출발할 때 밟고 있던 자석은 한 번 떨어져야 다시 세므로 바로 감속으로 오인하지 않는다.
     */
    if (expected && h->armed && h->stable_marker == 1) {
        h->event = expected;
        h->last_marker = expected;
        h->armed = 0;
    }
}
