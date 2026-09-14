#ifndef PSD_CONFIG_H
#define PSD_CONFIG_H
#include <stdint.h>
/* Confirmed passive piezo wiring. Set 1 only for an active buzzer. */
#define PSD_ACTIVE_BUZZER 0
#define PSD_REQUEST_HOLD_MS 500UL
#define PSD_RECOVERY_TIMEOUT_MS 15000UL
#define PSD_START_DEBOUNCE_MS 30UL
#define PSD_KW11_DEBOUNCE_MS 40UL
#define PSD_ESTOP_RELEASE_CONFIRM_MS 50UL
#define PSD_DOOR_OPEN_TIME_MS 1000UL
#define PSD_DOOR_OPEN_TIMEOUT_MS 60000UL
#define PSD_DOOR_CLOSE_TIMEOUT_MS 60000UL

/* ---- PSD 문 스텝모터: DRV8825 + BJ42D15, 풀스텝(M0~M2 미연결) = 200스텝/회전 ---- */
/* 0으로 바꾸면 EN을 계속 HIGH로 두어 모터에 전류가 흐르지 않는다. */
#define PSD_STEPPER_ENABLE 1
/* 시연값: 한 바퀴. 문 기구를 달면 실제로 열리는 데 필요한 스텝 수로 바꾼다. */
#define PSD_STEPPER_OPEN_STEPS 200
/* 문이 반대로 열리면 1 <-> 0 을 바꾼다. */
#define PSD_STEPPER_OPEN_DIR_HIGH 1
/* 최고 속도. 5000us/스텝 = 200스텝/초 = 1초에 한 바퀴. */
#define PSD_STEPPER_RUN_INTERVAL_US 5000UL
/* 출발·도착 직전의 느린 속도와, 그 사이를 잇는 가감속 스텝 수. */
#define PSD_STEPPER_START_INTERVAL_US 12000UL
#define PSD_STEPPER_RAMP_STEPS 20
/* 도착 후 이 시간이 지나면 EN을 HIGH로 올려 코일 전류를 끈다(대기 발열 방지). */
#define PSD_STEPPER_IDLE_RELEASE_MS 500UL

#if (PSD_STEPPER_START_INTERVAL_US > 32000UL) || \
    (PSD_STEPPER_RUN_INTERVAL_US < 1000UL) || \
    (PSD_STEPPER_RUN_INTERVAL_US > PSD_STEPPER_START_INTERVAL_US)
#error "Stepper intervals must satisfy 1000 <= RUN <= START <= 32000 us"
#endif
#if (PSD_STEPPER_RAMP_STEPS < 1) || (PSD_STEPPER_RAMP_STEPS > 250)
#error "PSD_STEPPER_RAMP_STEPS must be 1..250"
#endif
#if (PSD_STEPPER_OPEN_STEPS < 1) || (PSD_STEPPER_OPEN_STEPS > 30000)
#error "PSD_STEPPER_OPEN_STEPS must be 1..30000"
#endif
#endif
