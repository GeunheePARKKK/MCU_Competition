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
#include "motor.h"
#include "ui_display.h"
#include "lcd_safe.h"

/*
 * train_sketch + 실제 주행 모터. 원본 train_sketch와 달라진 점:
 *  - FSM의 모터 명령(주행/감속/정지)이 칩 5번(D3) PWM으로 실제 출력된다. 주행 120, 감속 80.
 *    자석은 1개씩 순서대로 센다: 출발 후 첫 번째 자석 = 감속, 두 번째 자석 = 정지.
 *    비상정지·통신 끊김·오류에서는 FSM이 정지 명령을 낸다.
 *  - 주행·감속 중에는 LCD를 상태가 바뀔 때만 갱신한다(홀센서 샘플링을 막지 않게).
 *  - 열차 문 단계 생략(TRAIN_DOOR_STEP_ENABLE 0): 정차 재확인 -> 바로 PSD 열기,
 *    카운트다운 끝 -> 바로 PSD 닫기. 열차 KW11은 V1(닫힘)에 둔 채로 쓴다.
 *  - 열차 문 서보(9·10번)는 계속 OFF.
 * 처음에는 반드시 바퀴를 띄우고 시험한다.
 */

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
static const motor_config_t motorConfig = {
    TRAIN_MOTOR_RUN_PWM_INITIAL, TRAIN_MOTOR_APPROACH_PWM_INITIAL
};

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
    // One LCD row takes ~30 ms and stalls hall sampling. While the train is moving,
    // redraw only on a state change so a passing magnet is not missed.
    static train_state_t drawnState = TRAIN_STATE_COUNT;
    const bool moving = fsm.state == TRAIN_STATE_RUNNING || fsm.state == TRAIN_STATE_APPROACH;
    if (moving && fsm.state == drawnState) return;
    if (!moving && uint32_t(now - lastLcd) < 100) return;
    lastLcd = now;
    drawnState = fsm.state;
    char top[17], bottom[17];
    uiFormatLcd(makeUi(), now, top, bottom);
    safeLcd.show(top, bottom);
}
void setup() {
    motor_init();                               // D3 LOW, Timer2 PWM ready, output OFF
    digitalWrite(10, LOW); pinMode(10, OUTPUT); // servo OFF, also SPI SS output
    digitalWrite(9, LOW); pinMode(9, OUTPUT);   // other reserved servo pin OFF
    pinMode(4, INPUT_PULLUP);                  // KW11 COM-GND, NC-D4: pressed HIGH
    pinMode(5, OUTPUT); digitalWrite(5, LOW);  // existing TRAIN communication LED
    pinMode(A0, INPUT); digitalWrite(A0, LOW);
    analogReference(DEFAULT);
    safeLcd.begin();
    char splash[17];
    snprintf_P(splash, sizeof splash, PSTR("MOTOR %u/%u ON"),
               unsigned(TRAIN_MOTOR_RUN_PWM_INITIAL), unsigned(TRAIN_MOTOR_APPROACH_PWM_INITIAL));
    safeLcd.show("TRAIN UI v5", TRAIN_MOTOR_ENABLED ? splash : "ALL MOTORS OFF");
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
    inputs.train_door_closed = kw11.closed; // virtual door input; keep TRAIN KW11 at V1
    inputs.hall_ok = !hall.rail_fault;
    // Single-magnet counting: every marker is one magnet.
    inputs.at_m1 = hall.stable_marker == 1;   // standing on a magnet (start position / station)
    inputs.m2_detected = hall.event == 2;     // 1st new magnet after departure -> slow down
    inputs.m4_detected = hall.event == 3;     // 2nd new magnet -> stop
    inputs.aligned = hall.stable_marker == 1; // 0.3 s stop re-check: still on that magnet
    hall.event = 0;
    inputs.local_fault_code = !comm.initialized ? FAULT_CODE_COMM :
                              hall.rail_fault ? FAULT_CODE_HALL : FAULT_CODE_NONE;
    comm_train_update(&comm, &outputs, now, &inputs);
    train_fsm_update(&fsm, &inputs, now, &outputs);
#if TRAIN_MOTOR_ENABLED
    // RUN/APPROACH only in RUNNING/APPROACH with both doors closed; STOP everywhere else.
    motor_apply_command((uint8_t)outputs.motor_command, &motorConfig);
#else
    motor_apply_command(MOTOR_CMD_STOP, &motorConfig);
#endif
    digitalWrite(10, LOW); // train door servos stay OFF
    digitalWrite(9, LOW);
    comm_train_transmit(&comm, &outputs, now);
    digitalWrite(5, inputs.communication_ok ? HIGH : LOW);
    updateLcd(now);
}
