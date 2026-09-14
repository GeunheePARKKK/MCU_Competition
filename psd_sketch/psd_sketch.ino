#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include "comm.h"
#include "debounce.h"
#include "door_toggle.h"
#include "timebase.h"
#include "psd_config.h"
#include "psd_fsm.h"
#include "ui_display.h"
#include "lcd_safe.h"
#include "ui_button.h"
#include "psd_outputs.h"

static psd_fsm_t fsm;
static psd_fsm_inputs_t inputs;
static psd_fsm_outputs_t outputs;
static comm_t comm;
static door_toggle_t kw11;
static debounce_t startButton;
static LiquidCrystal_I2C lcd(0x27, 16, 2);
static SafeLcd safeLcd(lcd);
static UiModeButton viewButton;
static PsdIndicators indicators;
static PsdOutputs hardwareOutputs;
static uint32_t lastLcd, estopReleaseSince;
static bool estopHeld, estopReleasePending;
static uint8_t readEstop(uint32_t now) {
    // Assert immediately; require a stable release. Switch wired D2 -> GND.
    if (digitalRead(2) == LOW) {
        estopHeld = true;
        estopReleasePending = false;
    } else if (estopHeld) {
        if (!estopReleasePending) { estopReleasePending = true; estopReleaseSince = now; }
        if ((uint32_t)(now - estopReleaseSince) >= PSD_ESTOP_RELEASE_CONFIRM_MS)
            estopHeld = false;
    }
    return estopHeld;
}
static UiSnapshot makeUi() {
    UiSnapshot s = {};
    s.trainLocal = false; s.normal = comm.ui_normal;
    s.trainState = inputs.train_state; s.psdState = outputs.state;
    s.countdown = comm.last_train_packet.countdown;
    s.fault = outputs.fault_code; s.peerFault = inputs.train_fault_code;
    s.closed = inputs.door_closed; s.estopPressed = inputs.estop_active;
    s.startHeld = debounce_is_active(&startButton);
    s.doorCommand = outputs.door_command;
    s.link = inputs.communication_ok; s.peerValid = inputs.peer_state_valid;
    s.radioResult = comm.radio_init_result; s.packetCheck = comm.last_packet_check;
    return s;
}
static void updateLcd(uint32_t now, const UiSnapshot &s) {
    if (uint32_t(now - lastLcd) < 100) return;
    lastLcd = now;
    char top[17], bottom[17];
    uiFormatLcd(s, now, top, bottom);
    safeLcd.show(top, bottom);
}
void setup() {
    digitalWrite(10, LOW); pinMode(10, OUTPUT); // servo OFF, also hardware SPI SS
    digitalWrite(9, LOW); pinMode(9, OUTPUT);
    hardwareOutputs.begin(); // Safe latch BEFORE LCD or radio initialization.
    pinMode(A3, INPUT_PULLUP); // VIEW only; no START/recovery/door side effects.
    pinMode(2, INPUT_PULLUP);  // physical E-STOP, active LOW
    pinMode(4, INPUT_PULLUP);  // KW11 NC: pressed HIGH
    pinMode(5, INPUT_PULLUP);  // START D5/PD5, active LOW (existing sketch wiring)
    safeLcd.begin();
    safeLcd.show("PSD UI v4", "ALL MOTORS OFF");
    uint32_t now = millis();
    viewButton.begin(digitalRead(A3) == LOW, now);
    door_toggle_init(&kw11, digitalRead(4), PSD_KW11_DEBOUNCE_MS, now);
    debounce_init(&startButton, digitalRead(5), DEBOUNCE_LEVEL_LOW,
                  PSD_START_DEBOUNCE_MS, now);
    psd_fsm_init(&fsm, &PSD_FSM_DEFAULT_CONFIG, now);
    outputs.state = PSD_STATE_INIT;
    comm_init(&comm, COMM_ROLE_PSD, millis());
}
void loop() {
    uint32_t now = millis();
    door_toggle_update(&kw11, digitalRead(4), now);
    debounce_update(&startButton, digitalRead(5), now);
    if (viewButton.update(digitalRead(A3) == LOW, now)) comm.ui_normal = !comm.ui_normal;
    inputs.init_complete = 1;
    inputs.door_closed = kw11.closed; // virtual test state, never a physical interlock
    inputs.start_pressed = debounce_take_pressed(&startButton);
    inputs.estop_active = readEstop(now);
    inputs.local_fault_code = comm.initialized ? FAULT_CODE_NONE : FAULT_CODE_COMM;
    comm_psd_update(&comm, &outputs, now, &inputs);
    psd_fsm_update(&fsm, &inputs, now, &outputs);
    digitalWrite(10, LOW); // servo outputs remain physically disabled
    digitalWrite(9, LOW);
    // Receive -> FSM -> new packet: E-STOP never sends a stale START request.
    comm_psd_transmit(&comm, &outputs, now);
    const UiSnapshot s = makeUi();
    hardwareOutputs.apply(indicators.update(s, now));
    updateLcd(now, s);
}
