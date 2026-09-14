#ifndef PSD_FSM_H
#define PSD_FSM_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#include "protocol.h"

/* PSD FSM이 문 드라이버에 요구하는 목표상태 */
typedef enum
{
    PSD_DOOR_HOLD = 0,
    PSD_DOOR_CLOSE,
    PSD_DOOR_OPEN,
    PSD_DOOR_RELEASE
} psd_door_command_t;

typedef struct
{
    uint32_t request_hold_ms;
    uint32_t door_open_motion_ms;
    uint32_t door_close_timeout_ms;
} psd_fsm_config_t;

/* start_pressed는 디바운싱 후 한 루프 동안만 1이 되는 이벤트다. */
typedef struct
{
    uint8_t init_complete;
    uint8_t communication_ok;
    uint8_t communication_timed_out;
    uint8_t peer_state_valid;
    uint8_t peer_reboot_detected;

    train_state_t train_state;
    psd_command_t train_command;
    fault_code_t train_fault_code;

    fault_code_t local_fault_code;
    uint8_t start_pressed;
    uint8_t estop_active;
    uint8_t door_closed;
} psd_fsm_inputs_t;

/* state, request, flags, fault_code는 PSD -> TRAIN 패킷에 바로 사용 가능하다. */
typedef struct
{
    psd_state_t state;
    fault_code_t fault_code;
    psd_door_command_t door_command;
    psd_request_t request;
    uint8_t flags;
} psd_fsm_outputs_t;

typedef struct
{
    psd_state_t state;
    fault_code_t fault_code;
    uint32_t state_enter_ms;
    psd_request_t active_request;
    uint32_t request_start_ms;
    psd_fsm_config_t config;
    uint8_t close_armed;
} psd_fsm_t;

extern const psd_fsm_config_t PSD_FSM_DEFAULT_CONFIG;

void psd_fsm_init(psd_fsm_t *fsm,
                  const psd_fsm_config_t *config,
                  uint32_t now_ms);

void psd_fsm_update(psd_fsm_t *fsm,
                    const psd_fsm_inputs_t *inputs,
                    uint32_t now_ms,
                    psd_fsm_outputs_t *outputs);

psd_state_t psd_fsm_get_state(const psd_fsm_t *fsm);
fault_code_t psd_fsm_get_fault(const psd_fsm_t *fsm);


#ifdef __cplusplus
}
#endif

#endif /* PSD_FSM_H */
