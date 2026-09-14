#include "psd_fsm.h"

#include "psd_config.h"

#include <stddef.h>

const psd_fsm_config_t PSD_FSM_DEFAULT_CONFIG =
{
    PSD_REQUEST_HOLD_MS,
    PSD_DOOR_OPEN_TIME_MS,
    PSD_DOOR_CLOSE_TIMEOUT_MS
};

static uint32_t elapsed_ms(uint32_t now_ms, uint32_t since_ms)
{
    return (uint32_t)(now_ms - since_ms);
}

static void enter_state(psd_fsm_t *fsm,
                        psd_state_t next_state,
                        uint32_t now_ms)
{
    fsm->state = next_state;
    fsm->state_enter_ms = now_ms;
    fsm->close_armed = 0U;
}

static void enter_fault(psd_fsm_t *fsm,
                        fault_code_t fault_code,
                        uint32_t now_ms)
{
    fsm->fault_code = fault_code;
    fsm->active_request = PSD_REQUEST_NONE;
    enter_state(fsm, PSD_STATE_FAULT, now_ms);
}

static void enter_estop(psd_fsm_t *fsm, uint32_t now_ms)
{
    fsm->fault_code = FAULT_CODE_ESTOP;
    fsm->active_request = PSD_REQUEST_NONE;
    enter_state(fsm, PSD_STATE_ESTOP, now_ms);
}

static void start_request(psd_fsm_t *fsm,
                          psd_request_t request,
                          uint32_t now_ms)
{
    fsm->active_request = request;
    fsm->request_start_ms = now_ms;
}

static uint8_t init_conditions_met(const psd_fsm_inputs_t *inputs)
{
    return (uint8_t)(
        inputs->init_complete &&
        inputs->communication_ok &&
        inputs->peer_state_valid &&
        ((inputs->train_state == TRAIN_STATE_INIT) ||
         (inputs->train_state == TRAIN_STATE_READY)) &&
        inputs->door_closed &&
        !inputs->estop_active &&
        (inputs->local_fault_code == FAULT_CODE_NONE));
}

static uint8_t recovery_allowed(const psd_fsm_inputs_t *inputs)
{
    return (uint8_t)(
        inputs->start_pressed &&
        inputs->door_closed &&
        !inputs->estop_active &&
        inputs->communication_ok &&
        (inputs->local_fault_code == FAULT_CODE_NONE));
}

static void make_outputs(const psd_fsm_t *fsm,
                         const psd_fsm_inputs_t *inputs,
                         psd_fsm_outputs_t *outputs)
{
    outputs->state = fsm->state;
    outputs->fault_code = fsm->fault_code;
    outputs->door_command = PSD_DOOR_HOLD;
    outputs->request = fsm->active_request;
    if ((outputs->request == PSD_REQUEST_START && fsm->state != PSD_STATE_READY) ||
        (outputs->request == PSD_REQUEST_RECOVERY && fsm->state != PSD_STATE_INIT))
        outputs->request = PSD_REQUEST_NONE;
    outputs->flags = PSD_FLAG_NONE;

    switch (fsm->state)
    {
        case PSD_STATE_INIT:
        case PSD_STATE_READY:
        case PSD_STATE_CLOSING:
        case PSD_STATE_CLOSED:
            outputs->door_command = PSD_DOOR_CLOSE;
            break;

        case PSD_STATE_OPENING:
        case PSD_STATE_OPEN:
            outputs->door_command = PSD_DOOR_OPEN;
            break;

        case PSD_STATE_FAULT:
            outputs->door_command = PSD_DOOR_HOLD;
            break;

        case PSD_STATE_ESTOP:
            outputs->door_command = PSD_DOOR_RELEASE;
            break;

        default:
            outputs->door_command = PSD_DOOR_HOLD;
            break;
    }

    if (inputs->door_closed)
    {
        outputs->flags |= PSD_FLAG_CLOSED_CONFIRMED;
    }

    /* 물리 입력 해제 후에도 RECOVERY 전까지 E-STOP 플래그를 유지한다. */
    if (inputs->estop_active || (fsm->state == PSD_STATE_ESTOP))
    {
        outputs->flags |= PSD_FLAG_ESTOP_ACTIVE;
    }
}

void psd_fsm_init(psd_fsm_t *fsm,
                  const psd_fsm_config_t *config,
                  uint32_t now_ms)
{
    if (fsm == NULL)
    {
        return;
    }

    fsm->config = (config != NULL) ? *config : PSD_FSM_DEFAULT_CONFIG;
    fsm->state = PSD_STATE_INIT;
    fsm->fault_code = FAULT_CODE_NONE;
    fsm->state_enter_ms = now_ms;
    fsm->active_request = PSD_REQUEST_NONE;
    fsm->request_start_ms = now_ms;
    fsm->close_armed = 0U;
}

void psd_fsm_update(psd_fsm_t *fsm,
                    const psd_fsm_inputs_t *inputs,
                    uint32_t now_ms,
                    psd_fsm_outputs_t *outputs)
{
    uint32_t elapsed;

    if ((fsm == NULL) || (inputs == NULL) || (outputs == NULL))
    {
        return;
    }

    if ((fsm->active_request == PSD_REQUEST_START) &&
        (elapsed_ms(now_ms, fsm->request_start_ms) >=
         fsm->config.request_hold_ms))
    {
        fsm->active_request = PSD_REQUEST_NONE;
    }

    /* 1순위: 물리 E-STOP */
    if (inputs->estop_active)
    {
        if (fsm->state != PSD_STATE_ESTOP)
        {
            enter_estop(fsm, now_ms);
        }
        make_outputs(fsm, inputs, outputs);
        return;
    }

    /* 첫 번째 START는 복구 요청이며 실제 출발 요청과 분리한다. */
    if (fsm->state == PSD_STATE_ESTOP)
    {
        if (recovery_allowed(inputs))
        {
            start_request(fsm, PSD_REQUEST_RECOVERY, now_ms);
            fsm->fault_code = FAULT_CODE_NONE;
            enter_state(fsm, PSD_STATE_INIT, now_ms);
        }
        make_outputs(fsm, inputs, outputs);
        return;
    }

    /* 2순위: 통신두절 또는 상대 재부팅 */
    if (inputs->communication_timed_out)
    {
        enter_fault(fsm, FAULT_CODE_COMM, now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    if (inputs->peer_reboot_detected)
    {
        enter_fault(fsm, FAULT_CODE_PEER_REBOOT, now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    if (fsm->state == PSD_STATE_FAULT)
    {
        if (recovery_allowed(inputs))
        {
            start_request(fsm, PSD_REQUEST_RECOVERY, now_ms);
            fsm->fault_code = FAULT_CODE_NONE;
            enter_state(fsm, PSD_STATE_INIT, now_ms);
        }
        make_outputs(fsm, inputs, outputs);
        return;
    }

    /* 3순위: 로컬 오류와 TRAIN 오류 */
    if (inputs->local_fault_code != FAULT_CODE_NONE)
    {
        enter_fault(fsm, inputs->local_fault_code, now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    /* RECOVERY 송신 중 INIT에서는 상대의 이전 오류상태가 사라지길 기다린다. */
    if ((fsm->state != PSD_STATE_INIT) &&
        inputs->peer_state_valid &&
        (inputs->train_state == TRAIN_STATE_ESTOP))
    {
        enter_estop(fsm, now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    if ((fsm->state != PSD_STATE_INIT) &&
        inputs->peer_state_valid &&
        (inputs->train_state == TRAIN_STATE_FAULT))
    {
        enter_fault(fsm,
                    (inputs->train_fault_code != FAULT_CODE_NONE)
                        ? inputs->train_fault_code
                        : FAULT_CODE_COMM,
                    now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    /* A completed close must remain confirmed until a new cycle. */
    if (fsm->state == PSD_STATE_CLOSED && !inputs->door_closed)
    {
        enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
        make_outputs(fsm, inputs, outputs);
        return;
    }

    elapsed = elapsed_ms(now_ms, fsm->state_enter_ms);

    switch (fsm->state)
    {
        case PSD_STATE_INIT:
            if (fsm->active_request == PSD_REQUEST_RECOVERY)
            {
                uint32_t request_age = elapsed_ms(now_ms, fsm->request_start_ms);
                if (request_age >= PSD_RECOVERY_TIMEOUT_MS)
                {
                    enter_fault(fsm, FAULT_CODE_COMM, now_ms);
                    break;
                }
                if (request_age < fsm->config.request_hold_ms ||
                    !inputs->peer_state_valid ||
                    (inputs->train_state != TRAIN_STATE_INIT &&
                     inputs->train_state != TRAIN_STATE_READY))
                    break;
                fsm->active_request = PSD_REQUEST_NONE;
            }
            if (init_conditions_met(inputs))
            {
                enter_state(fsm, PSD_STATE_READY, now_ms);
            }
            break;

        case PSD_STATE_READY:
            if (inputs->start_pressed && inputs->communication_ok &&
                inputs->door_closed && inputs->peer_state_valid &&
                inputs->train_state == TRAIN_STATE_READY)
            {
                start_request(fsm, PSD_REQUEST_START, now_ms);
            }

            if (inputs->peer_state_valid &&
                (inputs->train_state == TRAIN_STATE_WAIT_PSD_OPEN) &&
                (inputs->train_command == PSD_CMD_OPEN))
            {
                enter_state(fsm, PSD_STATE_OPENING, now_ms);
            }
            break;

        case PSD_STATE_OPENING:
            if (!inputs->door_closed && elapsed >= fsm->config.door_open_motion_ms)
            {
                enter_state(fsm, PSD_STATE_OPEN, now_ms);
            }
            else if (elapsed >= PSD_DOOR_OPEN_TIMEOUT_MS)
                enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
            break;

        case PSD_STATE_OPEN:
            if (inputs->peer_state_valid &&
                (inputs->train_state == TRAIN_STATE_WAIT_PSD_CLOSED) &&
                (inputs->train_command == PSD_CMD_CLOSE))
            {
                enter_state(fsm, PSD_STATE_CLOSING, now_ms);
            }
            break;

        case PSD_STATE_CLOSING:
            if (!inputs->door_closed)
                fsm->close_armed = 1U;
            if (inputs->door_closed && fsm->close_armed)
            {
                enter_state(fsm, PSD_STATE_CLOSED, now_ms);
            }
            else if (elapsed >= fsm->config.door_close_timeout_ms)
            {
                enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
            }
            break;

        case PSD_STATE_CLOSED:
            if (inputs->peer_state_valid &&
                (inputs->train_state == TRAIN_STATE_READY))
            {
                enter_state(fsm, PSD_STATE_READY, now_ms);
            }
            break;

        case PSD_STATE_FAULT:
        case PSD_STATE_ESTOP:
        default:
            break;
    }

    make_outputs(fsm, inputs, outputs);
}

psd_state_t psd_fsm_get_state(const psd_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->state : PSD_STATE_FAULT;
}

fault_code_t psd_fsm_get_fault(const psd_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->fault_code : FAULT_CODE_NONE;
}
