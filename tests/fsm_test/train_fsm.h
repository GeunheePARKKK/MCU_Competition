#ifndef TRAIN_FSM_H
#define TRAIN_FSM_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#include "protocol.h"

/* TRAIN FSM이 모터 드라이버에 요구하는 동작 모드 */
typedef enum
{
    MOTOR_COMMAND_STOP = 0,
    MOTOR_COMMAND_RUN,
    MOTOR_COMMAND_APPROACH
} motor_command_t;

/* TRAIN FSM이 열차문 드라이버에 요구하는 목표상태 */
typedef enum
{
    TRAIN_DOOR_HOLD = 0,
    TRAIN_DOOR_CLOSE,
    TRAIN_DOOR_OPEN,
    TRAIN_DOOR_RELEASE
} train_door_command_t;

/* 실제 기구 시험 후 조정할 TRAIN 상태머신 설정값 */
typedef struct
{
    uint32_t approach_timeout_ms;
    uint32_t coasting_wait_ms;
    uint32_t psd_open_timeout_ms;
    uint32_t train_door_open_ms;
    uint32_t dwell_ms;
    uint8_t countdown_start_seconds;
    uint32_t countdown_zero_hold_ms;
    uint32_t train_door_close_timeout_ms;
    uint32_t psd_close_timeout_ms;
} train_fsm_config_t;

/*
 * 메인 루프가 센서·통신 결과를 갱신한 뒤 FSM에 전달하는 입력.
 * 모든 논리값은 0=false, 0 이외=true로 사용한다.
 */
typedef struct
{
    uint8_t init_complete;
    uint8_t communication_ok;
    uint8_t communication_timed_out;
    uint8_t peer_state_valid;
    uint8_t peer_reboot_detected;

    psd_state_t psd_state;
    psd_request_t psd_request;
    fault_code_t psd_fault_code;
    uint8_t psd_estop_active;
    uint8_t psd_closed_confirmed;

    fault_code_t local_fault_code;
    uint8_t train_door_closed;
    uint8_t hall_ok;
    uint8_t at_m1;
    uint8_t m2_detected;
    uint8_t m4_detected;
    uint8_t aligned;
} train_fsm_inputs_t;

/* FSM 결과를 모터·문·통신 계층이 사용한다. */
typedef struct
{
    train_state_t state;
    fault_code_t fault_code;
    motor_command_t motor_command;
    train_door_command_t door_command;
    psd_command_t psd_command;
    uint8_t countdown;
} train_fsm_outputs_t;

typedef struct
{
    train_state_t state;
    fault_code_t fault_code;
    uint32_t state_enter_ms;
    train_fsm_config_t config;
    uint8_t close_armed;
} train_fsm_t;

extern const train_fsm_config_t TRAIN_FSM_DEFAULT_CONFIG;

void train_fsm_init(train_fsm_t *fsm,
                    const train_fsm_config_t *config,
                    uint32_t now_ms);

void train_fsm_update(train_fsm_t *fsm,
                      const train_fsm_inputs_t *inputs,
                      uint32_t now_ms,
                      train_fsm_outputs_t *outputs);

train_state_t train_fsm_get_state(const train_fsm_t *fsm);
fault_code_t train_fsm_get_fault(const train_fsm_t *fsm);


#ifdef __cplusplus
}
#endif

#endif /* TRAIN_FSM_H */
