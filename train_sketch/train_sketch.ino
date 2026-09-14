#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include "comm.h"
#include "debounce.h"
#include "timebase.h"
#include "door_toggle.h"
#include "train_config.h"
#include "train_fsm.h"
#include "hall_detector.h"
#include "ui_display.h"
#include "lcd_safe.h"

static train_fsm_t fsm;
static train_fsm_inputs_t inputs;
static train_fsm_outputs_t outputs;
static comm_t comm;
static door_toggle_t kw11;
static hall_detector_t hall;
static LiquidCrystal_I2C lcd(0x27, 16, 2);
static SafeLcd safeLcd(lcd);
static uint16_t adcWindow[4], adcSum;
static uint8_t adcIndex;
static uint32_t lastSample, lastLcd;

static uint8_t expectedMarker() {
    switch (fsm.state) {
    case TRAIN_STATE_INIT: case TRAIN_STATE_READY: case TRAIN_STATE_COMPLETE:
    case TRAIN_STATE_FAULT: case TRAIN_STATE_ESTOP: return 1;
    case TRAIN_STATE_RUNNING: return 2;
    case TRAIN_STATE_APPROACH: return 3;
    default: return 0;
    }
}
static UiSnapshot makeUi() {
    UiSnapshot s = {};
    s.trainLocal = true; s.normal = comm.ui_normal;
    s.trainState = outputs.state; s.psdState = inputs.psd_state;
    s.countdown = outputs.countdown; s.fault = outputs.fault_code;
    s.peerFault = inputs.psd_fault_code; s.closed = inputs.train_door_closed;
    s.peerClosed = inputs.psd_closed_confirmed;
    s.estopPressed = inputs.psd_estop_active;
    s.link = inputs.communication_ok; s.peerValid = inputs.peer_state_valid;
    s.radioResult = comm.radio_init_result; s.packetCheck = comm.last_packet_check;
    s.doorCommand = outputs.door_command;
    s.adc = hall.adc; s.delta = hall.delta; s.marker = hall.stable_marker;
    s.expected = expectedMarker();
    return s;
}
static void updateLcd(uint32_t now) {
    if (uint32_t(now - lastLcd) < 100) return;
    lastLcd = now;
    char top[17], bottom[17];
    uiFormatLcd(makeUi(), now, top, bottom);
    safeLcd.show(top, bottom);
}
void setup() {
    digitalWrite(3, LOW); pinMode(3, OUTPUT);   // motor safe before any LCD delays
    digitalWrite(10, LOW); pinMode(10, OUTPUT); // servo OFF, also SPI SS output
    digitalWrite(9, LOW); pinMode(9, OUTPUT);   // other reserved servo pin OFF
    pinMode(4, INPUT_PULLUP);                  // KW11 COM-GND, NC-D4: pressed HIGH
    pinMode(5, OUTPUT); digitalWrite(5, LOW);  // existing TRAIN communication LED
    pinMode(A0, INPUT); digitalWrite(A0, LOW);
    analogReference(DEFAULT);
    safeLcd.begin();
    safeLcd.show("TRAIN UI v4", "ALL MOTORS OFF");
    uint32_t now = millis();
    door_toggle_init(&kw11, digitalRead(4), TRAIN_KW11_DEBOUNCE_MS, now);
    hall_detector_init(&hall, now);
    uint16_t first = analogRead(A0);
    adcSum = first * 4U;
    for (uint8_t i = 0; i < 4; ++i) adcWindow[i] = first;
    train_fsm_init(&fsm, &TRAIN_FSM_DEFAULT_CONFIG, now);
    outputs.state = TRAIN_STATE_INIT;
    outputs.countdown = COUNTDOWN_INACTIVE;
    comm_init(&comm, COMM_ROLE_TRAIN, millis());
    lastSample = millis();
}
void loop() {
    uint32_t now = millis();
    door_toggle_update(&kw11, digitalRead(4), now);
    if ((uint32_t)(now - lastSample) >= TRAIN_HALL_SAMPLE_PERIOD_MS) {
        lastSample = now;
        adcSum -= adcWindow[adcIndex];
        adcWindow[adcIndex] = analogRead(A0);
        adcSum += adcWindow[adcIndex];
        adcIndex = (adcIndex + 1) % 4;
        hall_detector_update(&hall, (adcSum + 2U) / 4U, expectedMarker(), now);
    }
    inputs.init_complete = 1;
    inputs.train_door_closed = kw11.closed; // virtual test input; physical motors OFF
    inputs.hall_ok = !hall.rail_fault;
    inputs.at_m1 = hall.stable_marker == 1 && hall.last_marker == 1;
    inputs.m2_detected = hall.event == 2;
    inputs.m4_detected = hall.event == 3; // old M4 name = the new 3-magnet stop marker
    inputs.aligned = hall.stable_marker == 3;
    hall.event = 0;
    inputs.local_fault_code = !comm.initialized ? FAULT_CODE_COMM :
                              hall.rail_fault ? FAULT_CODE_HALL : FAULT_CODE_NONE;
    comm_train_update(&comm, &outputs, now, &inputs);
    train_fsm_update(&fsm, &inputs, now, &outputs);
    // HARD OFF: virtual door inputs must not authorize physical motion.
    digitalWrite(3, LOW);
    digitalWrite(10, LOW);
    digitalWrite(9, LOW);
    comm_train_transmit(&comm, &outputs, now);
    digitalWrite(5, inputs.communication_ok ? HIGH : LOW);
    updateLcd(now);
}
