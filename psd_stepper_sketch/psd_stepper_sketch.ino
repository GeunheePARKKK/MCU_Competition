#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include "comm.h"
#include "debounce.h"
#include "door_stepper.h"
#include "door_toggle.h"
#include "timebase.h"
#include "psd_config.h"
#include "psd_fsm.h"
#include "ui_display.h"
#include "lcd_safe.h"
#include "psd_outputs.h"

/*
 * psd_sketch + 문 스텝모터(DRV8825). 원본 psd_sketch와 달라진 점:
 *  - 칩 15번(D9)=STEP, 26번(A3)=DIR, 2번(D0)=EN. FSM의 문 명령으로 스텝모터가 실제로 움직인다.
 *  - 26번(A3) 화면전환 버튼 코드 삭제(버튼 미장착, 이제 DIR 출력).
 *  - 문 닫힘 판정은 여전히 KW11 가상 V0/V1이다. 스텝모터 위치로 대신하지 않는다.
 *  - 전원을 켤 때 문이 닫힌 위치에 있어야 한다(원점 센서 없음).
 */

static psd_fsm_t fsm;
static psd_fsm_inputs_t inputs;
static psd_fsm_outputs_t outputs;
static comm_t comm;
static door_toggle_t kw11;
static debounce_t startButton;
static LiquidCrystal_I2C lcd(0x27, 16, 2);
static SafeLcd safeLcd(lcd);
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
    door_stepper_init(); // EN HIGH first: door motor stays unpowered until the FSM asks.
    digitalWrite(10, LOW); pinMode(10, OUTPUT); // 16번 servo OFF, also hardware SPI SS
    hardwareOutputs.begin(); // Safe latch BEFORE LCD or radio initialization.
    pinMode(2, INPUT_PULLUP);  // physical E-STOP, active LOW
    pinMode(4, INPUT_PULLUP);  // KW11 NC: pressed HIGH
    pinMode(5, INPUT_PULLUP);  // START D5/PD5, active LOW (existing sketch wiring)
    safeLcd.begin();
    safeLcd.show("PSD UI v5", PSD_STEPPER_ENABLE ? "DOOR STEPPER ON" : "ALL MOTORS OFF");
    uint32_t now = millis();
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
    inputs.init_complete = 1;
    inputs.door_closed = kw11.closed; // virtual test state, never a physical interlock
    inputs.start_pressed = debounce_take_pressed(&startButton);
    inputs.estop_active = readEstop(now);
    inputs.local_fault_code = comm.initialized ? FAULT_CODE_NONE : FAULT_CODE_COMM;
    comm_psd_update(&comm, &outputs, now, &inputs);
    psd_fsm_update(&fsm, &inputs, now, &outputs);
    digitalWrite(10, LOW); // 16번 servo stays disabled
    door_stepper_update(outputs.door_command, now);
    // Receive -> FSM -> new packet: E-STOP never sends a stale START request.
    comm_psd_transmit(&comm, &outputs, now);
    const UiSnapshot s = makeUi();
    hardwareOutputs.apply(indicators.update(s, now));
    updateLcd(now, s);
}
