#ifndef TRAIN_PSD_LCD_SAFE_H
#define TRAIN_PSD_LCD_SAFE_H
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

class SafeLcd {
public:
    explicit SafeLcd(LiquidCrystal_I2C &lcd) : lcd_(lcd) {}
    void begin() {
        Wire.begin();
        Wire.setWireTimeout(3000, true);
        Wire.beginTransmission(0x27);
        available_ = Wire.endTransmission() == 0;
        if (available_) { lcd_.init(); lcd_.backlight(); }
        if (Wire.getWireTimeoutFlag()) available_ = false;
        Wire.clearWireTimeoutFlag();
    }
    void show(const char *top, const char *bottom) {
        if (!available_) return;
        row(0, top);
        if (available_) row(1, bottom);
    }
private:
    LiquidCrystal_I2C &lcd_;
    bool available_ = false;
    char previous_[2][17] = {{0}, {0}};
    void row(uint8_t r, const char *text) {
        if (!strcmp(previous_[r], text)) return;
        const char *p = text;
        lcd_.setCursor(0, r);
        for (uint8_t i = 0; i < 16; ++i) {
            lcd_.write(*p ? *p++ : ' ');
            if (Wire.getWireTimeoutFlag()) {
                available_ = false;
                Wire.clearWireTimeoutFlag();
                return;
            }
        }
        strncpy(previous_[r], text, 16);
        previous_[r][16] = 0;
    }
};
#endif
