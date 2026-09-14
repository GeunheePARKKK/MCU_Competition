#ifndef TRAIN_PSD_INDICATORS_H
#define TRAIN_PSD_INDICATORS_H
#include "ui_model.h"
#include <avr/pgmspace.h>

// Confirmed wiring: 595 output -> 1k -> LED anode, cathode -> GND.
// Nine LEDs are active HIGH. Segments A..G remain active HIGH as before.
static const uint16_t PSD_LED_MASK = 0x01FF;
static const uint16_t PSD_INDICATORS_OFF = 0x0000;
static const uint8_t PSD_DIGIT_MASKS[6] PROGMEM = {0x3F,0x06,0x5B,0x4F,0x66,0x6D};
struct PsdIndicatorFrame { uint16_t word, hz; };

inline bool uiStationPhase(train_state_t s) {
    switch (s) {
    case TRAIN_STATE_DOOR_OPENING: case TRAIN_STATE_WAIT_PSD_OPEN:
    case TRAIN_STATE_DWELL: case TRAIN_STATE_COUNTDOWN:
    case TRAIN_STATE_DOOR_CLOSING: case TRAIN_STATE_WAIT_PSD_CLOSED:
    case TRAIN_STATE_COMPLETE: return true;
    default: return false;
    }
}

class PsdIndicators {
public:
    PsdIndicatorFrame update(const UiSnapshot &s, uint32_t now) {
        const uint8_t alarm = uiAlarm(s), count = uiCount(s);
        uint16_t leds = s.link ? (1U << 1) : 0U;
        uint8_t segments = 0;
        if (alarm) {
            if (alarm == 1 || (now % 500UL) < 250UL) leds |= (1U << 8);
            if (alarm == 1) segments = 0x79; // E. E-STOP keeps the digit blank.
        } else if (s.link && s.peerValid) {
            if (s.trainState == TRAIN_STATE_READY && s.psdState == PSD_STATE_READY && s.closed)
                leds |= (1U << 0);
            if (s.trainState == TRAIN_STATE_RUNNING) leds |= (1U << 2);
            if (s.trainState == TRAIN_STATE_APPROACH) leds |= (1U << 3);
            if (uiStationPhase(s.trainState)) leds |= (1U << 4);
            if (s.trainState == TRAIN_STATE_DOOR_OPENING || s.trainState == TRAIN_STATE_DOOR_CLOSING)
                leds |= (1U << 5);
            if (s.psdState == PSD_STATE_OPENING || s.psdState == PSD_STATE_CLOSING)
                leds |= (1U << 6);
            if (s.trainState == TRAIN_STATE_COMPLETE && s.psdState == PSD_STATE_CLOSED && s.closed)
                leds |= (1U << 7);
            if (count != COUNTDOWN_INACTIVE) segments = pgm_read_byte(&PSD_DIGIT_MASKS[count]);
        }

        if (alarm != lastAlarm_) {
            sound_ = alarm == 2 ? ESTOP_SOUND : alarm == 1 ? FAULT_SOUND : SILENT;
            soundSince_ = now;
        }
        if (!alarm) {
            if (!s.link || !s.peerValid) sound_ = SILENT;
            else if (count != COUNTDOWN_INACTIVE &&
                     (lastTrain_ != TRAIN_STATE_COUNTDOWN || count != lastCount_)) {
                sound_ = count == 0 ? COUNT_ZERO : COUNT_TICK; soundSince_ = now;
            } else if (s.trainState == TRAIN_STATE_DOOR_OPENING && lastTrain_ != s.trainState) {
                sound_ = ARRIVAL; soundSince_ = now;
            } else if (s.trainState == TRAIN_STATE_COMPLETE && s.psdState == PSD_STATE_CLOSED &&
                       lastTrain_ != s.trainState) {
                sound_ = DONE; soundSince_ = now;
            }
        }
        lastAlarm_ = alarm;
        lastTrain_ = s.link && s.peerValid ? s.trainState : TRAIN_STATE_INIT;
        lastCount_ = count;
        const uint32_t age = uint32_t(now - soundSince_);
        uint16_t hz = 0;
        switch (sound_) {
        case ESTOP_SOUND: hz = age % 300UL < 150UL ? 2000 : 0; break;
        case FAULT_SOUND: hz = age % 2000UL < 150UL ? 700 : 0; break;
        case COUNT_TICK: hz = age < 100UL ? 1800 : 0; break;
        case COUNT_ZERO: hz = age < 200UL ? 2200 : 0; break;
        case ARRIVAL: hz = age < 150UL ? 1200 : age >= 250UL && age < 400UL ? 1600 : 0; break;
        case DONE:
            if (age < 100UL) hz = 1000;
            else if (age >= 150UL && age < 250UL) hz = 1500;
            else if (age >= 300UL && age < 450UL) hz = 2000;
            break;
        default: break;
        }
        PsdIndicatorFrame f = {
            uint16_t((leds & PSD_LED_MASK) | (uint16_t(segments) << 9)), hz
        };
        return f;
    }
private:
    enum Sound : uint8_t { SILENT, ARRIVAL, COUNT_TICK, COUNT_ZERO, DONE, FAULT_SOUND, ESTOP_SOUND };
    Sound sound_ = SILENT;
    train_state_t lastTrain_ = TRAIN_STATE_INIT;
    uint8_t lastCount_ = COUNTDOWN_INACTIVE, lastAlarm_ = 0;
    uint32_t soundSince_ = 0;
};
#endif
