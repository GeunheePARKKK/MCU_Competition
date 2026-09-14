#ifndef TRAIN_PSD_UI_MODEL_H
#define TRAIN_PSD_UI_MODEL_H
#include <stdint.h>
#include "protocol.h"

// Read-only presentation snapshot. Never fed back into the safety FSM.
struct UiSnapshot {
    train_state_t trainState;
    psd_state_t psdState;
    uint8_t countdown, fault, peerFault, radioResult, packetCheck;
    uint8_t closed, peerClosed, estopPressed, startHeld, doorCommand;
    uint8_t marker, expected;
    uint16_t adc, delta;
    bool trainLocal, normal, link, peerValid;
};
inline uint8_t uiAlarm(const UiSnapshot &s) {
    if (s.estopPressed ||
        (s.trainLocal ? s.trainState == TRAIN_STATE_ESTOP : s.psdState == PSD_STATE_ESTOP) ||
        (s.link && s.peerValid &&
         (s.trainLocal ? s.psdState == PSD_STATE_ESTOP : s.trainState == TRAIN_STATE_ESTOP)))
        return 2;
    return s.fault || (s.link && s.peerValid && s.peerFault) ? 1 : 0;
}
inline uint8_t uiFault(const UiSnapshot &s) {
    return uiAlarm(s) == 2 ? uint8_t(FAULT_CODE_ESTOP) :
        s.fault ? s.fault : s.link && s.peerValid ? s.peerFault : uint8_t(FAULT_CODE_NONE);
}
inline uint8_t uiCount(const UiSnapshot &s) {
    return !uiAlarm(s) && s.link && (s.trainLocal || s.peerValid) &&
        s.trainState == TRAIN_STATE_COUNTDOWN && s.countdown <= COUNTDOWN_MAX_SECONDS
        ? s.countdown : COUNTDOWN_INACTIVE;
}
#endif
