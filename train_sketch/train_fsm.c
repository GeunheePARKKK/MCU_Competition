#include "train_fsm.h"

#include "train_config.h"

#include <stddef.h>

const train_fsm_config_t TRAIN_FSM_DEFAULT_CONFIG =
{
    TRAIN_APPROACH_TIMEOUT_MS,
    TRAIN_COASTING_WAIT_MS,
    TRAIN_PSD_OPEN_TIMEOUT_MS,
    TRAIN_DOOR_OPEN_TIME_MS,
    TRAIN_DWELL_TIME_MS,
    TRAIN_COUNTDOWN_START_SECONDS,
    TRAIN_COUNTDOWN_ZERO_HOLD_MS,
    TRAIN_DOOR_CLOSE_TIMEOUT_MS,
    TRAIN_PSD_CLOSE_TIMEOUT_MS
};

static uint32_t elapsed_ms(uint32_t now_ms, uint32_t since_ms)
{
    return (uint32_t)(now_ms - since_ms);
}

static void enter_state(train_fsm_t *fsm,
                        train_state_t next_state,
                        uint32_t now_ms)
{
    fsm->state = next_state;
    fsm->state_enter_ms = now_ms;
    fsm->close_armed = 0U;
}

static void enter_fault(train_fsm_t *fsm,
                        fault_code_t fault_code,
                        uint32_t now_ms)
{
    fsm->fault_code = fault_code;
    enter_state(fsm, TRAIN_STATE_FAULT, now_ms);
}

static void enter_estop(train_fsm_t *fsm, uint32_t now_ms)
{
    fsm->fault_code = FAULT_CODE_ESTOP;
    enter_state(fsm, TRAIN_STATE_ESTOP, now_ms);
}

static uint8_t ready_conditions_met(const train_fsm_inputs_t *inputs)
{
    return (uint8_t)(
        inputs->init_complete &&
        inputs->communication_ok &&
        inputs->peer_state_valid &&
        (inputs->psd_state == PSD_STATE_READY) &&
        (inputs->psd_fault_code == FAULT_CODE_NONE) &&
        !inputs->psd_estop_active &&
        inputs->train_door_closed &&
        inputs->psd_closed_confirmed &&
        inputs->hall_ok &&
        inputs->at_m1 &&
        (inputs->local_fault_code == FAULT_CODE_NONE));
}

static uint8_t complete_conditions_met(const train_fsm_inputs_t *inputs)
{
    return (uint8_t)(
        inputs->communication_ok &&
        inputs->peer_state_valid &&
        (inputs->psd_state == PSD_STATE_CLOSED) &&
        (inputs->psd_fault_code == FAULT_CODE_NONE) &&
        !inputs->psd_estop_active &&
        inputs->train_door_closed &&
        inputs->psd_closed_confirmed &&
        inputs->hall_ok &&
        inputs->at_m1 &&
        (inputs->local_fault_code == FAULT_CODE_NONE));
}

static uint8_t recovery_allowed(const train_fsm_inputs_t *inputs)
{
    return (uint8_t)(
        (inputs->psd_request == PSD_REQUEST_RECOVERY) &&
        inputs->communication_ok &&
        inputs->peer_state_valid &&
        inputs->train_door_closed && inputs->psd_closed_confirmed &&
        inputs->hall_ok && inputs->at_m1 &&
        !inputs->psd_estop_active &&
        (inputs->local_fault_code == FAULT_CODE_NONE));
}

static uint8_t is_motion_state(train_state_t state)
{
    return (uint8_t)((state == TRAIN_STATE_RUNNING) ||
                     (state == TRAIN_STATE_APPROACH));
}

static void make_outputs(const train_fsm_t *fsm,
                         const train_fsm_inputs_t *inputs,
                         uint32_t now_ms,
                         train_fsm_outputs_t *outputs)
{
    uint32_t elapsed;
    uint32_t whole_seconds;

    outputs->state = fsm->state;
    outputs->fault_code = fsm->fault_code;
    outputs->motor_command = MOTOR_COMMAND_STOP;
    outputs->door_command = TRAIN_DOOR_HOLD;
    outputs->psd_command = PSD_CMD_NONE;
    outputs->countdown = COUNTDOWN_INACTIVE;

    switch (fsm->state)
    {
        case TRAIN_STATE_INIT:
        case TRAIN_STATE_READY:
        case TRAIN_STATE_RUNNING:
        case TRAIN_STATE_APPROACH:
        case TRAIN_STATE_COASTING:
            outputs->door_command = TRAIN_DOOR_CLOSE;
            break;

        case TRAIN_STATE_DOOR_OPENING:
        case TRAIN_STATE_WAIT_PSD_OPEN:
        case TRAIN_STATE_DWELL:
        case TRAIN_STATE_COUNTDOWN:
            outputs->door_command = TRAIN_DOOR_OPEN;
            break;

        case TRAIN_STATE_DOOR_CLOSING:
        case TRAIN_STATE_WAIT_PSD_CLOSED:
        case TRAIN_STATE_COMPLETE:
            outputs->door_command = TRAIN_DOOR_CLOSE;
            break;

        case TRAIN_STATE_FAULT:
            outputs->door_command = TRAIN_DOOR_HOLD;
            break;

        case TRAIN_STATE_ESTOP:
            outputs->door_command = TRAIN_DOOR_RELEASE;
            break;

        default:
            outputs->door_command = TRAIN_DOOR_HOLD;
            break;
    }

    /* 문 두 쪽이 모두 닫힌 경우에만 주행 PWM을 허용한다. */
    if (inputs->train_door_closed && inputs->psd_closed_confirmed)
    {
        if (fsm->state == TRAIN_STATE_RUNNING)
        {
            outputs->motor_command = MOTOR_COMMAND_RUN;
        }
        else if (fsm->state == TRAIN_STATE_APPROACH)
        {
            outputs->motor_command = MOTOR_COMMAND_APPROACH;
        }
    }

    if (fsm->state == TRAIN_STATE_WAIT_PSD_OPEN)
    {
        outputs->psd_command = PSD_CMD_OPEN;
    }
    else if (fsm->state == TRAIN_STATE_WAIT_PSD_CLOSED)
    {
        outputs->psd_command = PSD_CMD_CLOSE;
    }

    if (fsm->state == TRAIN_STATE_COUNTDOWN)
    {
        elapsed = elapsed_ms(now_ms, fsm->state_enter_ms);
        whole_seconds = elapsed / 1000UL;

        if (whole_seconds < fsm->config.countdown_start_seconds)
        {
            outputs->countdown = (uint8_t)(
                fsm->config.countdown_start_seconds - whole_seconds);
        }
        else
        {
            outputs->countdown = 0U;
        }
    }
}

void train_fsm_init(train_fsm_t *fsm,
                    const train_fsm_config_t *config,
                    uint32_t now_ms)
{
    if (fsm == NULL)
    {
        return;
    }

    fsm->config = (config != NULL) ? *config : TRAIN_FSM_DEFAULT_CONFIG;
    fsm->fault_code = FAULT_CODE_NONE;
    fsm->state = TRAIN_STATE_INIT;
    fsm->state_enter_ms = now_ms;
    fsm->close_armed = 0U;
}

void train_fsm_update(train_fsm_t *fsm,
                      const train_fsm_inputs_t *inputs,
                      uint32_t now_ms,
                      train_fsm_outputs_t *outputs)
{
    uint32_t elapsed;
    uint32_t countdown_ms;

    if ((fsm == NULL) || (inputs == NULL) || (outputs == NULL))
    {
        return;
    }

    /* 1순위: E-STOP */
    if (inputs->psd_estop_active ||
        (inputs->peer_state_valid &&
         (inputs->psd_state == PSD_STATE_ESTOP)))
    {
        if (fsm->state != TRAIN_STATE_ESTOP)
        {
            enter_estop(fsm, now_ms);
        }
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    if (fsm->state == TRAIN_STATE_ESTOP)
    {
        if (recovery_allowed(inputs))
        {
            fsm->fault_code = FAULT_CODE_NONE;
            enter_state(fsm, TRAIN_STATE_INIT, now_ms);
        }
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    /* 2순위: 통신두절 또는 상대 재부팅 */
    if (inputs->communication_timed_out)
    {
        enter_fault(fsm, FAULT_CODE_COMM, now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    if (inputs->peer_reboot_detected)
    {
        enter_fault(fsm, FAULT_CODE_PEER_REBOOT, now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    /* FAULT에서는 원인이 제거되고 RECOVERY가 들어올 때만 INIT으로 간다. */
    if (fsm->state == TRAIN_STATE_FAULT)
    {
        if (recovery_allowed(inputs))
        {
            fsm->fault_code = FAULT_CODE_NONE;
            enter_state(fsm, TRAIN_STATE_INIT, now_ms);
        }
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    /* 3순위: 로컬 또는 상대 일반 오류 */
    if (inputs->local_fault_code != FAULT_CODE_NONE)
    {
        enter_fault(fsm, inputs->local_fault_code, now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    if ((fsm->state != TRAIN_STATE_INIT) &&
        inputs->peer_state_valid &&
        (inputs->psd_state == PSD_STATE_FAULT))
    {
        enter_fault(fsm,
                    (inputs->psd_fault_code != FAULT_CODE_NONE)
                        ? inputs->psd_fault_code
                        : FAULT_CODE_PSD_DOOR,
                    now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    /* 주행 중 어느 한 문이라도 닫힘 확인이 사라지면 즉시 FAULT. */
    if (is_motion_state(fsm->state))
    {
        if (!inputs->train_door_closed)
        {
            enter_fault(fsm, FAULT_CODE_TRAIN_DOOR, now_ms);
            make_outputs(fsm, inputs, now_ms, outputs);
            return;
        }
        if (!inputs->psd_closed_confirmed)
        {
            enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
            make_outputs(fsm, inputs, now_ms, outputs);
            return;
        }
    }

    /* Recheck throughout peer closing and after DONE, not just once. */
    if ((fsm->state == TRAIN_STATE_WAIT_PSD_CLOSED ||
         fsm->state == TRAIN_STATE_COMPLETE) && !inputs->train_door_closed)
    {
        enter_fault(fsm, FAULT_CODE_TRAIN_DOOR, now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }
    if (fsm->state == TRAIN_STATE_COMPLETE && !inputs->psd_closed_confirmed)
    {
        enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
        make_outputs(fsm, inputs, now_ms, outputs);
        return;
    }

    elapsed = elapsed_ms(now_ms, fsm->state_enter_ms);

    switch (fsm->state)
    {
        case TRAIN_STATE_INIT:
            if (ready_conditions_met(inputs))
            {
                enter_state(fsm, TRAIN_STATE_READY, now_ms);
            }
            break;

        case TRAIN_STATE_READY:
            if ((inputs->psd_request == PSD_REQUEST_START) &&
                ready_conditions_met(inputs))
            {
                enter_state(fsm, TRAIN_STATE_RUNNING, now_ms);
            }
            break;

        case TRAIN_STATE_RUNNING:
            if (inputs->m2_detected)
            {
                enter_state(fsm, TRAIN_STATE_APPROACH, now_ms);
            }
            else if (elapsed >= TRAIN_RUNNING_TIMEOUT_MS)
                enter_fault(fsm, FAULT_CODE_HALL, now_ms);
            break;

        case TRAIN_STATE_APPROACH:
            if (inputs->m4_detected)
            {
                enter_state(fsm, TRAIN_STATE_COASTING, now_ms);
            }
            else if (elapsed >= fsm->config.approach_timeout_ms)
            {
                enter_fault(fsm, FAULT_CODE_HALL, now_ms);
            }
            break;

        case TRAIN_STATE_COASTING:
            if (elapsed >= fsm->config.coasting_wait_ms)
            {
                if (inputs->hall_ok && inputs->aligned)
                {
                    enter_state(fsm, TRAIN_STATE_DOOR_OPENING, now_ms);
                }
                else
                {
                    enter_fault(fsm, FAULT_CODE_ALIGN, now_ms);
                }
            }
            break;

        case TRAIN_STATE_WAIT_PSD_OPEN:
            if (inputs->peer_state_valid &&
                (inputs->psd_state == PSD_STATE_OPEN))
            {
                enter_state(fsm, TRAIN_STATE_DWELL, now_ms);
            }
            else if (elapsed >= fsm->config.psd_open_timeout_ms)
            {
                enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
            }
            break;

        case TRAIN_STATE_DOOR_OPENING:
            if (!inputs->train_door_closed &&
                elapsed >= fsm->config.train_door_open_ms)
            {
                enter_state(fsm, TRAIN_STATE_WAIT_PSD_OPEN, now_ms);
            }
            else if (elapsed >= TRAIN_DOOR_OPEN_TIMEOUT_MS)
                enter_fault(fsm, FAULT_CODE_TRAIN_DOOR, now_ms);
            break;

        case TRAIN_STATE_DWELL:
            if (elapsed >= fsm->config.dwell_ms)
            {
                enter_state(fsm, TRAIN_STATE_COUNTDOWN, now_ms);
            }
            break;

        case TRAIN_STATE_COUNTDOWN:
            countdown_ms =
                ((uint32_t)fsm->config.countdown_start_seconds * 1000UL) +
                fsm->config.countdown_zero_hold_ms;
            if (elapsed >= countdown_ms)
            {
                enter_state(fsm, TRAIN_STATE_DOOR_CLOSING, now_ms);
            }
            break;

        case TRAIN_STATE_DOOR_CLOSING:
            if (!inputs->train_door_closed)
                fsm->close_armed = 1U;
            if (inputs->train_door_closed && fsm->close_armed)
            {
                enter_state(fsm, TRAIN_STATE_WAIT_PSD_CLOSED, now_ms);
            }
            else if (elapsed >= fsm->config.train_door_close_timeout_ms)
            {
                enter_fault(fsm, FAULT_CODE_TRAIN_DOOR, now_ms);
            }
            break;

        case TRAIN_STATE_WAIT_PSD_CLOSED:
            if (inputs->communication_ok && inputs->peer_state_valid &&
                inputs->train_door_closed && inputs->psd_closed_confirmed &&
                inputs->psd_state == PSD_STATE_CLOSED &&
                inputs->psd_fault_code == FAULT_CODE_NONE && !inputs->psd_estop_active)
            {
                enter_state(fsm, TRAIN_STATE_COMPLETE, now_ms);
            }
            else if (elapsed >= fsm->config.psd_close_timeout_ms)
            {
                enter_fault(fsm, FAULT_CODE_PSD_DOOR, now_ms);
            }
            break;

        case TRAIN_STATE_COMPLETE:
            if (complete_conditions_met(inputs))
            {
                enter_state(fsm, TRAIN_STATE_READY, now_ms);
            }
            break;

        case TRAIN_STATE_FAULT:
        case TRAIN_STATE_ESTOP:
        default:
            break;
    }

    make_outputs(fsm, inputs, now_ms, outputs);
}

train_state_t train_fsm_get_state(const train_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->state : TRAIN_STATE_FAULT;
}

fault_code_t train_fsm_get_fault(const train_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->fault_code : FAULT_CODE_NONE;
}
