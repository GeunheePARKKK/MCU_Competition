#ifndef TRAIN_CONFIG_H
#define TRAIN_CONFIG_H
#include <stdint.h>
/*
 * train_motor_sketch 전용: 주행 모터 출력을 켠다(원본 train_sketch는 계속 OFF).
 * 문 닫힘은 아직 KW11 가상 입력이다. 가상 입력이 실제 주행을 허가하는 상태이므로
 * 처음에는 반드시 바퀴를 띄우고 시험하고, 손 닿는 곳에 모터 전원 차단 스위치를 둔다.
 * 0으로 바꾸면 모터 핀(D3)을 계속 LOW로 둔다.
 */
#ifndef TRAIN_MOTOR_ENABLED
#define TRAIN_MOTOR_ENABLED 1
#endif
/* 사용자 지정: 주행 120, 감속 80. train_actuator_test에서 80~200 모두 회전 확인. */
#define TRAIN_MOTOR_RUN_PWM_INITIAL 120U
#define TRAIN_MOTOR_APPROACH_PWM_INITIAL 80U
/*
 * 0 = 열차 문 단계 생략(열차 문이 아직 가상 입력이라서):
 *     정차 재확인 -> 바로 PSD 열기, 카운트다운 끝 -> 바로 PSD 닫기.
 *     열차 KW11은 V1(닫힘)에 둔 채로 쓴다. 주행 허가·완료 확인 조건으로는 계속 쓰인다.
 * 1 = 원래 순서(열차 문 먼저 열고 닫기). 열차 문 서보를 실제로 달면 1로 되돌린다.
 */
#ifndef TRAIN_DOOR_STEP_ENABLE
#define TRAIN_DOOR_STEP_ENABLE 0
#endif
/*
 * 자석 1개씩 순서대로 세는 방식. 극성·세기와 상관없이 "있다/없다"만 본다.
 *   출발 위치(역)에 서 있음 -> 출발 후 첫 번째 자석 = 감속 -> 두 번째 자석 = 정차.
 * D = |ADC - BASELINE|. D >= PRESENT 이면 자석 있음, D <= CLEAR 이면 자석 없음,
 * 그 사이는 판단 보류(값이 흔들려도 오판하지 않게 하는 여유 구간).
 * 자석 1개를 실제 설치 거리에서 댔을 때 D가 PRESENT보다 확실히 커야 한다.
 */
#define TRAIN_HALL_BASELINE 508U
#define TRAIN_HALL_PRESENT_DELTA 15U
#define TRAIN_HALL_SAMPLE_PERIOD_MS 5UL
#define TRAIN_HALL_STABLE_TIME_MS 30UL
#define TRAIN_HALL_CLEAR_DELTA 8U
#if TRAIN_HALL_PRESENT_DELTA < (TRAIN_HALL_CLEAR_DELTA + 3U)
#error "TRAIN_HALL_PRESENT_DELTA must be at least CLEAR_DELTA + 3 (noise margin)"
#endif
#define TRAIN_MARKER_REARM_TIME_MS 150UL
#define TRAIN_HALL_RAIL_TIME_MS 250UL
#define TRAIN_KW11_DEBOUNCE_MS 40UL
/* Hand-operated test timeouts, NOT final track timings. */
#define TRAIN_RUNNING_TIMEOUT_MS 120000UL
#define TRAIN_APPROACH_TIMEOUT_MS 120000UL
#define TRAIN_COASTING_WAIT_MS 300UL
#define TRAIN_PSD_OPEN_TIMEOUT_MS 65000UL
#define TRAIN_DOOR_OPEN_TIME_MS 1000UL
#define TRAIN_DOOR_OPEN_TIMEOUT_MS 60000UL
#define TRAIN_DWELL_TIME_MS 5000UL
#define TRAIN_COUNTDOWN_START_SECONDS 5U
#define TRAIN_COUNTDOWN_ZERO_HOLD_MS 300UL
#define TRAIN_DOOR_CLOSE_TIMEOUT_MS 60000UL
#define TRAIN_PSD_CLOSE_TIMEOUT_MS 65000UL
#endif
