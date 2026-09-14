#ifndef LCD_STATUS_H
#define LCD_STATUS_H
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "protocol.h"
inline const char *trainLabel(uint8_t state) {
    switch (state) {
    case TRAIN_STATE_INIT: return "INIT";
    case TRAIN_STATE_READY: return "READY";
    case TRAIN_STATE_RUNNING: return "RUN";
    case TRAIN_STATE_APPROACH: return "SLOW";
    case TRAIN_STATE_COASTING: return "STOP";
    case TRAIN_STATE_DOOR_OPENING: return "T-OPEN";
    case TRAIN_STATE_WAIT_PSD_OPEN: return "P-OPEN";
    case TRAIN_STATE_DWELL: return "DWELL";
    case TRAIN_STATE_COUNTDOWN: return "COUNT";
    case TRAIN_STATE_DOOR_CLOSING: return "T-CLOSE";
    case TRAIN_STATE_WAIT_PSD_CLOSED: return "P-CLOSE";
    case TRAIN_STATE_COMPLETE: return "DONE";
    case TRAIN_STATE_FAULT: return "FAULT";
    case TRAIN_STATE_ESTOP: return "ESTOP";
    default: return "?";
    }
}
inline const char *psdLabel(uint8_t state) {
    switch (state) {
    case PSD_STATE_INIT: return "INIT";
    case PSD_STATE_READY: return "READY";
    case PSD_STATE_OPENING: return "OPENING";
    case PSD_STATE_OPEN: return "OPEN";
    case PSD_STATE_CLOSING: return "CLOSING";
    case PSD_STATE_CLOSED: return "CLOSED";
    case PSD_STATE_FAULT: return "FAULT";
    case PSD_STATE_ESTOP: return "ESTOP";
    default: return "?";
    }
}
inline char doorLabel(uint8_t command) {
    return command == 1 ? 'C' : command == 2 ? 'O' : command == 3 ? 'R' : 'H';
}
inline void lcdLine(LiquidCrystal_I2C &lcd, uint8_t row, const char *text) {
    char padded[17];
    uint8_t i = 0;
    while (i < 16 && text[i]) { padded[i] = text[i]; ++i; }
    while (i < 16) padded[i++] = ' ';
    padded[16] = 0;
    lcd.setCursor(0, row);
    lcd.print(padded);
}
#endif

