#ifndef TRAIN_PSD_UI_DISPLAY_H
#define TRAIN_PSD_UI_DISPLAY_H
#include <stdio.h>
#include <avr/pgmspace.h>
#include "ui_model.h"

inline const char *uiTrainLabel(train_state_t s) {
    switch (s) {
    case TRAIN_STATE_INIT: return PSTR("INIT");
    case TRAIN_STATE_READY: return PSTR("READY");
    case TRAIN_STATE_RUNNING: return PSTR("RUN");
    case TRAIN_STATE_APPROACH: return PSTR("SLOW");
    case TRAIN_STATE_COASTING: return PSTR("STOP");
    case TRAIN_STATE_DOOR_OPENING: return PSTR("T-OPEN");
    case TRAIN_STATE_WAIT_PSD_OPEN: return PSTR("P-OPEN");
    case TRAIN_STATE_DWELL: return PSTR("DWELL");
    case TRAIN_STATE_COUNTDOWN: return PSTR("COUNT");
    case TRAIN_STATE_DOOR_CLOSING: return PSTR("T-CLOSE");
    case TRAIN_STATE_WAIT_PSD_CLOSED: return PSTR("P-CLOSE");
    case TRAIN_STATE_COMPLETE: return PSTR("DONE");
    case TRAIN_STATE_FAULT: return PSTR("FAULT");
    case TRAIN_STATE_ESTOP: return PSTR("ESTOP");
    default: return PSTR("?");
    }
}
inline const char *uiPsdLabel(psd_state_t s) {
    switch (s) {
    case PSD_STATE_INIT: return PSTR("INIT");
    case PSD_STATE_READY: return PSTR("READY");
    case PSD_STATE_OPENING: return PSTR("OPENING");
    case PSD_STATE_OPEN: return PSTR("OPEN");
    case PSD_STATE_CLOSING: return PSTR("CLOSING");
    case PSD_STATE_CLOSED: return PSTR("CLOSED");
    case PSD_STATE_FAULT: return PSTR("FAULT");
    case PSD_STATE_ESTOP: return PSTR("ESTOP");
    default: return PSTR("?");
    }
}
inline char uiDoor(uint8_t cmd) {
    return cmd == 1 ? 'C' : cmd == 2 ? 'O' : cmd == 3 ? 'R' : 'H';
}
inline const char *uiNormalTitle(train_state_t s) {
    switch (s) {
    case TRAIN_STATE_READY: return PSTR("READY");
    case TRAIN_STATE_RUNNING: return PSTR("RUNNING");
    case TRAIN_STATE_APPROACH: return PSTR("SLOWING DOWN");
    case TRAIN_STATE_COASTING: return PSTR("STOP CHECK");
    case TRAIN_STATE_DOOR_OPENING: return PSTR("TRAIN OPENING");
    case TRAIN_STATE_WAIT_PSD_OPEN: return PSTR("PSD OPENING");
    case TRAIN_STATE_DWELL: return PSTR("DOORS OPEN");
    case TRAIN_STATE_DOOR_CLOSING: return PSTR("TRAIN CLOSING");
    case TRAIN_STATE_WAIT_PSD_CLOSED: return PSTR("PSD CLOSING");
    case TRAIN_STATE_COMPLETE: return PSTR("COMPLETE");
    default: return PSTR("PREPARING");
    }
}
// Strings remain in flash on ATmega328P. Both output buffers must be >=17 bytes.
inline void uiFormatLcd(const UiSnapshot &s, uint32_t now, char *top, char *bottom) {
    const char mode = s.normal ? 'N' : 'D';
    const char board = s.trainLocal ? 'T' : 'P';
    const uint8_t alarm = uiAlarm(s), count = uiCount(s);
    const char marker = s.marker <= 3 ? '0' + s.marker : '?';
    const bool page = (now / 2000UL) % 2 != 0;
    if (alarm) {
        // Fault number and link are NEVER replaced by an ADC page.
        snprintf_P(top, 17, PSTR("%c %c:%S F%u L%u"), mode, board,
                   alarm == 2 ? PSTR("ESTOP") : PSTR("FAULT"), unsigned(uiFault(s)), unsigned(s.link));
        if (s.normal) {
            if (s.trainLocal)
                snprintf_P(bottom, 17, PSTR("V%u P%u M%c REC:D5"), unsigned(s.closed), unsigned(s.peerClosed), marker);
            else
                snprintf_P(bottom, 17, PSTR("E%u V%u START=REC"), unsigned(s.estopPressed), unsigned(s.closed));
        } else if (s.trainLocal && !page) {
            snprintf_P(bottom, 17, PSTR("A%u D%u M%c"), unsigned(s.adc), unsigned(s.delta), marker);
        } else if (s.trainLocal) {
            snprintf_P(bottom, 17, PSTR("V%u P%u R%u K%u"), unsigned(s.closed), unsigned(s.peerClosed),
                       unsigned(s.radioResult), unsigned(s.packetCheck));
        } else if (!page) {
            snprintf_P(bottom, 17, PSTR("E%u V%u B%u T%u"), unsigned(s.estopPressed), unsigned(s.closed),
                       unsigned(s.startHeld), unsigned(s.peerFault));
        } else {
            snprintf_P(bottom, 17, PSTR("R%u K%u REC:D5"), unsigned(s.radioResult), unsigned(s.packetCheck));
        }
        return;
    }
    if (count != COUNTDOWN_INACTIVE) {
        if (s.normal) {
            snprintf_P(top, 17, PSTR("N CLOSE IN %u SEC"), unsigned(count));
            snprintf_P(bottom, 17, PSTR("SIM: MOTORS OFF"));
        } else {
            snprintf_P(top, 17, PSTR("D %c:COUNT %u L%u"), board, unsigned(count), unsigned(s.link));
            snprintf_P(bottom, 17, PSTR("V%u D%c C:%u"), unsigned(s.closed), uiDoor(s.doorCommand), unsigned(count));
        }
        return; // Includes zero; never alternate countdown away.
    }
    if (s.normal) {
        const bool initializing = s.trainLocal ? s.trainState == TRAIN_STATE_INIT : s.psdState == PSD_STATE_INIT;
        snprintf_P(top, 17, PSTR("N %S"), !s.link ? PSTR("WAIT LINK") :
                   initializing ? PSTR("PREPARING") : uiNormalTitle(s.trainState));
        snprintf_P(bottom, 17, PSTR("SIM: MOTORS OFF"));
        return;
    }
    if (s.trainLocal) {
        snprintf_P(top, 17, PSTR("D T:%S L%uE%u"), uiTrainLabel(s.trainState), unsigned(s.link), unsigned(s.expected));
        if (!page)
            snprintf_P(bottom, 17, PSTR("A%u D%u M%c"), unsigned(s.adc), unsigned(s.delta), marker);
        else
            snprintf_P(bottom, 17, PSTR("V%u D%c P%u MOT:OFF"), unsigned(s.closed), uiDoor(s.doorCommand), unsigned(s.peerClosed));
    } else {
        snprintf_P(top, 17, PSTR("D P:%S L%u"), uiPsdLabel(s.psdState), unsigned(s.link));
        if (!page)
            snprintf_P(bottom, 17, PSTR("T:%S V%u D%c"), s.peerValid ? uiTrainLabel(s.trainState) : PSTR("?"),
                       unsigned(s.closed), uiDoor(s.doorCommand));
        else
            snprintf_P(bottom, 17, PSTR("V%u B%u E%u R%u K%u"), unsigned(s.closed), unsigned(s.startHeld),
                       unsigned(s.estopPressed), unsigned(s.radioResult), unsigned(s.packetCheck));
    }
}
#endif
