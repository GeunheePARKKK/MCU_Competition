#ifndef TRAIN_PSD_OUTPUTS_H
#define TRAIN_PSD_OUTPUTS_H
#include <Arduino.h>
#include "psd_indicators.h"
#include "psd_config.h"

class PsdOutputs {
public:
    void begin() {
        pin(3, HIGH); // /OE: external 10k pull-up also required during reset/upload.
        pin(6, LOW);
        pin(A0, LOW); pin(A1, LOW); pin(A2, LOW);
        write(PSD_INDICATORS_OFF); // All off before enabling. U1=00, U2=00.
        digitalWrite(3, LOW);
    }
    void apply(const PsdIndicatorFrame &f) {
        if (f.word != word_) write(f.word);
        if (f.hz == hz_) return;
        noTone(6); digitalWrite(6, LOW);
        if (f.hz) {
#if PSD_ACTIVE_BUZZER
            digitalWrite(6, HIGH);
#else
            tone(6, f.hz); // Timer2 on PSD only. TRAIN motor output stays OFF.
#endif
        }
        hz_ = f.hz;
    }
private:
    uint16_t word_ = PSD_INDICATORS_OFF, hz_ = 0;
    static void pin(uint8_t n, uint8_t level) { digitalWrite(n, level); pinMode(n, OUTPUT); }
    void write(uint16_t word) {
        digitalWrite(A2, LOW);
        // U2 byte FIRST, U1 byte LAST. U1 pin9 -> U2 pin14.
        shiftOut(A0, A1, MSBFIRST, uint8_t(word >> 8));
        shiftOut(A0, A1, MSBFIRST, uint8_t(word));
        digitalWrite(A2, HIGH);
        word_ = word;
    }
};
#endif
