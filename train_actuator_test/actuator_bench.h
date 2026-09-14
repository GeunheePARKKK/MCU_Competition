#ifndef ACTUATOR_BENCH_H
#define ACTUATOR_BENCH_H
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <util/atomic.h>
#include <stdio.h>
#include "lcd_safe.h"

#if !defined(__AVR_ATmega328P__) || F_CPU != 16000000UL
#error "This bench sketch requires ATmega328P at 16 MHz (Arduino Uno)."
#endif
#if BENCH_TRAIN != 0 && BENCH_TRAIN != 1
#error "BENCH_TRAIN must be 0 (PSD) or 1 (TRAIN)."
#endif
#if BENCH_TRAIN != 1
#error "This one-button bench is TRAIN only. Use the separate PSD sketch."
#endif

// No radio, Hall/FSM decisions, virtual-door permission or EEPROM calibration.
// ONE button: DIP physical pin 4 = Arduino D2 / PD2, active LOW to GND.
// Short press (<400 ms) selects on release; >=1000 ms runs while held.
// D2 is NOT a separate E-stop in this standalone TRAIN test.
// Existing KW11 (Arduino D4 / DIP 6) and A3 are not used for control.
static const uint8_t MENU_COUNT = BENCH_TRAIN ? 10 : 6;
static const uint16_t SERVO_US[] = {1500, 1400, 1600};
static const uint8_t MOTOR_PWM[] = {80, 120, 160, 200};
static LiquidCrystal_I2C benchLcd(0x27, 16, 2);
static SafeLcd benchScreen(benchLcd);
static uint8_t selected = 0; // DIP 16 first, then DIP 15, then motor
static volatile uint8_t activeOutput = 0; // 0 OFF, 1 D9, 2 D10, 3 motor
static volatile uint8_t framesLeft = 0;
static volatile uint8_t stopReason = 0; // 1 button released, 4 lease
static bool ready = false, trackingPress = false, consumed = false, wasRunning = false;
static bool rawPressed = false, stablePressed = false;
static uint32_t changedAt = 0, heldAt = 0, lastScreen = 0;

// Caller must mask interrupts, or already be inside an ISR.
static void outputsOffUnsafe(uint8_t reason) {
    TCCR1A = _BV(WGM11); // disconnect both servo compare outputs
    PORTB &= ~(_BV(PB1) | _BV(PB2));
#if BENCH_TRAIN
    TCCR2A = _BV(WGM21) | _BV(WGM20); // disconnect motor OC2B
    OCR2B = 0;
    PORTD &= ~_BV(PD3);
#endif
    if (activeOutput) stopReason = reason;
    activeOutput = 0;
    framesLeft = 0;
}
static uint8_t rawStopReason() {
    return (PIND & _BV(PD2)) ? 1 : 0;
}

// 20 ms lease/input check independent of LCD/main-loop progress.
// NOT a physical power cut; cannot protect against MCU/interrupt failure.
ISR(TIMER1_OVF_vect) {
    if (!activeOutput) return;
    const uint8_t reason = rawStopReason();
    if (reason) outputsOffUnsafe(reason);
    else if (!framesLeft || --framesLeft == 0) outputsOffUnsafe(4);
}

static void stopOutputs(uint8_t reason) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { outputsOffUnsafe(reason); }
}
static void beginOutput() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        outputsOffUnsafe(0);
        // Check again after any preceding LCD/serial work.
        if (!rawStopReason()) {
            TCNT1 = 0;
            TIFR1 = _BV(TOV1);
            stopReason = 0;
            if (selected < 6) {
                const uint16_t ticks = SERVO_US[selected % 3] * 2U;
                OCR1A = ticks; OCR1B = ticks;
                activeOutput = selected < 3 ? 2 : 1;
                framesLeft = 50; // at most 1 s of commands
                TCCR1A = _BV(WGM11) | (selected < 3 ? _BV(COM1B1) : _BV(COM1A1));
            }
#if BENCH_TRAIN
            else if (selected < MENU_COUNT) {
                OCR2B = MOTOR_PWM[selected - 6];
                activeOutput = 3;
                framesLeft = 35; // at most 0.7 s, no automatic repeat
                TCCR2A = _BV(WGM21) | _BV(WGM20) | _BV(COM2B1);
            }
#endif
        }
    }
    if (activeOutput) {
        Serial.print(F("START ")); Serial.println(selected);
        wasRunning = true;
    }
}
static void showScreen(uint32_t now) {
    if (uint32_t(now - lastScreen) < 100) return;
    lastScreen = now;
    char top[17], bottom[17];
    const char board = BENCH_TRAIN ? 'T' : 'P';
    if (selected < 6)
        snprintf(top, sizeof(top), "%c P%u %uus %s", board,
                 selected < 3 ? 16U : 15U, SERVO_US[selected % 3], activeOutput ? "ON" : "OFF");
    else
        snprintf(top, sizeof(top), "%c MOTOR %u %s", board,
                 MOTOR_PWM[selected - 6], activeOutput ? "ON" : "OFF");
    if (activeOutput) snprintf(bottom, sizeof(bottom), "RUN B1 %ums", framesLeft * 20U);
    else if (rawPressed && trackingPress && !consumed)
        snprintf(bottom, sizeof(bottom), "B1 HOLD %lums", (unsigned long)(now - heldAt));
    else if (rawPressed) snprintf(bottom, sizeof(bottom), "RELEASE BUTTON");
    else snprintf(bottom, sizeof(bottom), "B0 TAP / HOLD1s");
    benchScreen.show(top, bottom);
}
void setup() {
    // Outputs first, before potentially slow LCD initialization.
    digitalWrite(9, LOW); pinMode(9, OUTPUT);
    digitalWrite(10, LOW); pinMode(10, OUTPUT);
    digitalWrite(3, BENCH_TRAIN ? LOW : HIGH); pinMode(3, OUTPUT);
    digitalWrite(6, LOW); pinMode(6, OUTPUT); // PSD buzzer silent
    digitalWrite(7, LOW); pinMode(7, OUTPUT); // radio CE inactive
    digitalWrite(8, HIGH); pinMode(8, OUTPUT); // radio deselected
    digitalWrite(5, LOW); // TRAIN comm LED off; PSD START remains input
    pinMode(5, BENCH_TRAIN ? OUTPUT : INPUT_PULLUP);
    pinMode(4, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);
    pinMode(A3, INPUT_PULLUP);
    Serial.begin(115200);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR1A = 0; TCCR1B = 0; TIMSK1 = 0;
        TCNT1 = 0; ICR1 = 39999; OCR1A = 3000; OCR1B = 3000;
        TCCR1A = _BV(WGM11);
        TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11); // /8, mode 14, 50 Hz
        TIFR1 = _BV(TOV1); TIMSK1 = _BV(TOIE1);
#if BENCH_TRAIN
        TCCR2A = _BV(WGM21) | _BV(WGM20);
        TCCR2B = _BV(CS22); OCR2B = 0; TIMSK2 = 0; // /64, 976.56 Hz
#endif
    }
    benchScreen.begin();
    rawPressed = stablePressed = digitalRead(2) == LOW;
    changedAt = millis(); // require a fresh stable release, including after boot
    Serial.println(F("BENCH TRAIN ONE BUTTON P4"));
}
void loop() {
    const uint32_t now = millis();
    const bool pressed = digitalRead(2) == LOW;
    if (!pressed) {
        // Do not debounce stopping. A brief opening during a run also consumes
        // that gesture so contact bounce cannot restart it.
        if (activeOutput) consumed = true;
        stopOutputs(1);
    }
    if (wasRunning && !activeOutput) {
        Serial.print(F("STOP ")); Serial.println(stopReason);
        wasRunning = false;
    }
    if (pressed != rawPressed) { rawPressed = pressed; changedAt = now; }
    if (rawPressed != stablePressed && uint32_t(now - changedAt) >= 40) {
        stablePressed = rawPressed;
        if (stablePressed) {
            trackingPress = ready;
            ready = false; consumed = false;
            heldAt = changedAt;
        } else {
            const uint32_t duration = changedAt - heldAt;
            if (trackingPress && !consumed && duration < 400) {
                selected = (selected + 1) % MENU_COUNT;
                Serial.print(F("SELECT ")); Serial.println(selected);
            }
            trackingPress = false; consumed = false;
        }
    }
    if (!rawPressed && !stablePressed && uint32_t(now - changedAt) >= 80) ready = true;
    if (rawPressed && stablePressed && trackingPress && !consumed &&
        uint32_t(now - heldAt) >= 1000) {
        consumed = true; // even if the final raw recheck rejects starting
        beginOutput();
    }
    showScreen(now);
}
#endif
