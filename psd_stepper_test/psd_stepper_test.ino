#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include "door_stepper.h"
#include "psd_config.h"
#include "lcd_safe.h"

/*
 * PSD 문 스텝모터 단독 시험 — 무선·FSM·TRAIN 없이 PSD 보드 하나로 돌린다.
 *  칩 11번(D5) START 한 번 = 열기(한 바퀴), 다시 한 번 = 닫기(한 바퀴 되돌아옴).
 *  칩 4번(D2) 비상정지를 누르고 있는 동안 = 즉시 정지 + 코일 전류 차단(손으로 돌아감).
 *  비상정지를 떼도 저절로 다시 움직이지 않는다. 그다음 START는 무조건 닫기다.
 *  부팅 때 START를 누르고 있었다면 한 번 뗀 뒤부터 인식한다.
 *  LED·세그먼트·부저·무선은 꺼 둔다.
 */

static LiquidCrystal_I2C lcd(0x27, 16, 2);
static SafeLcd safeLcd(lcd);
static psd_door_command_t command = PSD_DOOR_CLOSE;
static bool startRaw, startStable, startArmed;
static uint32_t startChanged, lastLcd;

/* 눌렀을 때 한 번만 true. 30ms 이상 안정돼야 인정한다. */
static bool takeStartPress(uint32_t now) {
    const bool pressed = digitalRead(5) == LOW;
    if (pressed != startRaw) { startRaw = pressed; startChanged = now; }
    if (startRaw == startStable || uint32_t(now - startChanged) < 30) return false;
    startStable = startRaw;
    if (!startStable) { startArmed = true; return false; }
    if (!startArmed) return false;
    startArmed = false;
    return true;
}

static void updateLcd(uint32_t now, bool estop) {
    if (uint32_t(now - lastLcd) < 200) return;
    lastLcd = now;
    char top[17], bottom[17];
    snprintf_P(top, sizeof top, PSTR("POS %4d / %d"), door_stepper_position(), PSD_STEPPER_OPEN_STEPS);
    PGM_P state;
    if (estop) state = PSTR("ESTOP FREE");
    else if (door_stepper_is_moving()) state = command == PSD_DOOR_OPEN ? PSTR("OPENING") : PSTR("CLOSING");
    else if (command == PSD_DOOR_OPEN) state = PSTR("OPEN");
    else if (command == PSD_DOOR_CLOSE) state = PSTR("CLOSED");
    else state = PSTR("STOPPED");
    snprintf_P(bottom, sizeof bottom, PSTR("%-10S EN%u"), state, unsigned(door_stepper_is_energized()));
    safeLcd.show(top, bottom);
}

void setup() {
    door_stepper_init();                        // EN HIGH first
    digitalWrite(10, LOW); pinMode(10, OUTPUT); // 16번 servo OFF, keep SPI SS an output
    digitalWrite(3, HIGH); pinMode(3, OUTPUT);  // 595 /OE HIGH: LEDs and digit off
    digitalWrite(6, LOW);  pinMode(6, OUTPUT);  // buzzer off
    digitalWrite(7, LOW);  pinMode(7, OUTPUT);  // nRF CE inactive
    digitalWrite(8, HIGH); pinMode(8, OUTPUT);  // nRF CSN deselected
    pinMode(2, INPUT_PULLUP);                   // E-STOP, chip pin 4
    pinMode(5, INPUT_PULLUP);                   // START, chip pin 11
    safeLcd.begin();
    safeLcd.show("PSD STEP TEST", "D5 OPEN/CLOSE");
    delay(1500);
    startRaw = startStable = digitalRead(5) == LOW;
    startArmed = !startStable;
    startChanged = millis();
}

void loop() {
    const uint32_t now = millis();
    const bool estop = digitalRead(2) == LOW;

    if (estop) command = PSD_DOOR_RELEASE;
    else if (command == PSD_DOOR_RELEASE) command = PSD_DOOR_HOLD;

    if (takeStartPress(now) && !estop)
        command = (command == PSD_DOOR_CLOSE) ? PSD_DOOR_OPEN : PSD_DOOR_CLOSE;

    door_stepper_update(command, now);
    updateLcd(now, estop);
}
