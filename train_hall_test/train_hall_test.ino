#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include "lcd_safe.h"

#if !defined(__AVR_ATmega328P__) || F_CPU != 16000000UL
#error Use Arduino Uno / ATmega328P at 16 MHz for this TRAIN-only test.
#endif

// Physical DIP-28 pins: H1=23 (A0), H2=24 (A1).
// Standalone measurement only: no radio, movement, buttons or marker decisions.
static const uint8_t HALL_PINS[2] = {A0, A1};
static LiquidCrystal_I2C lcd(0x27, 16, 2);
static SafeLcd screen(lcd);
static uint32_t lastFrameMs;

static void outputsOff() {
    digitalWrite(3, LOW);  // TRAIN motor, physical pin 5.
    digitalWrite(9, LOW);  // Servo, physical pin 15.
    digitalWrite(10, LOW); // Servo, physical pin 16.
    digitalWrite(7, LOW);  // Radio CE inactive, physical pin 13.
    digitalWrite(8, HIGH); // Radio CSN deselected, physical pin 14.
}

static uint16_t readHall(uint8_t pin) {
    // Discard the first conversion after switching ADC channels.
    (void)analogRead(pin);
    uint16_t total = 0;
    for (uint8_t i = 0; i < 8; ++i) total += analogRead(pin);
    return (total + 4) / 8;
}

static void showReadings() {
    const uint16_t h1 = readHall(HALL_PINS[0]);
    const uint16_t h2 = readHall(HALL_PINS[1]);
    char top[17], bottom[17];
    // '!' flags a near-rail reading, not a diagnosed fault or disconnected sensor.
    snprintf(top, sizeof(top), "H1 P23 A=%4u%c", h1,
             (h1 <= 5 || h1 >= 1018) ? '!' : ' ');
    snprintf(bottom, sizeof(bottom), "H2 P24 A=%4u%c", h2,
             (h2 <= 5 || h2 >= 1018) ? '!' : ' ');
    screen.show(top, bottom);
    // Optional debug output. USBasp does not provide a serial monitor port.
    Serial.print(F("H1=")); Serial.print(h1);
    Serial.print(F(",H2=")); Serial.println(h2);
}

void setup() {
    // Set output latches before changing pin direction, before LCD startup.
    outputsOff();
    const uint8_t outputs[] = {3, 9, 10, 7, 8};
    for (uint8_t i = 0; i < sizeof(outputs); ++i) pinMode(outputs[i], OUTPUT);
    for (uint8_t i = 0; i < 2; ++i) {
        pinMode(HALL_PINS[i], INPUT);
        digitalWrite(HALL_PINS[i], LOW); // No internal pull-up on analog outputs.
    }
    analogReference(DEFAULT); // AVCC: same 5 V rail as the Hall sensors.
    Serial.begin(115200);
    screen.begin();
    showReadings();
    lastFrameMs = millis();
}

void loop() {
    outputsOff();
    const uint32_t now = millis();
    if ((uint32_t)(now - lastFrameMs) < 200) return;
    lastFrameMs = now;
    showReadings();
}
