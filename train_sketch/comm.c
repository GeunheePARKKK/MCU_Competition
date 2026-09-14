#include "comm.h"

#include <stddef.h>
#include <string.h>

#include "radio_config.h"
#include "timebase.h"

#define NRF_RX_FIFO_DEPTH UINT8_C(3)

static uint8_t time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (uint8_t)((int32_t)(now_ms - deadline_ms) >= 0);
}

static uint8_t comm_has_timed_out(const comm_t *comm, uint32_t now_ms)
{
    return (uint8_t)(
        comm->has_valid_rx && timebase_has_elapsed(now_ms,
                             comm->last_valid_rx_ms,
                             COMM_TIMEOUT_MS));
}

static void saturating_increment(uint16_t *value)
{
    if (*value != UINT16_MAX)
    {
        ++(*value);
    }
}

static void service_tx_result(comm_t *comm, uint32_t now_ms)
{
    nrf24_result_t result = nrf24_update_tx(now_ms);

    if (result == NRF24_RESULT_TX_SUCCESS)
    {
        comm->last_tx_result = result;
        saturating_increment(&comm->tx_success_count);
    }
    else if ((result == NRF24_RESULT_MAX_RETRY) ||
             (result == NRF24_RESULT_TIMEOUT))
    {
        comm->last_tx_result = result;
        saturating_increment(&comm->tx_failure_count);
    }
}

static void receive_for_train(comm_t *comm, uint32_t now_ms)
{
    uint8_t raw[NRF_PAYLOAD_SIZE];
    uint8_t packet_index;
    psd_to_train_packet_t packet;
    nrf24_result_t radio_result;
    packet_check_result_t check_result;

    for (packet_index = 0U;
         packet_index < NRF_RX_FIFO_DEPTH;
         ++packet_index)
    {
        radio_result = nrf24_receive(raw);
        if (radio_result != NRF24_RESULT_OK)
        {
            break;
        }

        memcpy(&packet, raw, sizeof(packet));
        check_result = packet_validate_psd_to_train(&packet);
        comm->last_packet_check = check_result;

        if (check_result == PACKET_CHECK_OK)
        {
            comm->last_psd_packet = packet;
            comm->has_valid_rx = 1U;
            comm->last_valid_rx_ms = now_ms;
            saturating_increment(&comm->valid_rx_count);
        }
        else
        {
            saturating_increment(&comm->invalid_rx_count);
        }
    }
}

static void receive_for_psd(comm_t *comm, uint32_t now_ms)
{
    uint8_t raw[NRF_PAYLOAD_SIZE];
    uint8_t packet_index;
    train_to_psd_packet_t packet;
    nrf24_result_t radio_result;
    packet_check_result_t check_result;

    for (packet_index = 0U;
         packet_index < NRF_RX_FIFO_DEPTH;
         ++packet_index)
    {
        radio_result = nrf24_receive(raw);
        if (radio_result != NRF24_RESULT_OK)
        {
            break;
        }

        memcpy(&packet, raw, sizeof(packet));
        check_result = packet_validate_train_to_psd(&packet);
        comm->last_packet_check = check_result;

        if (check_result == PACKET_CHECK_OK)
        {
            comm->last_train_packet = packet;
            comm->has_valid_rx = 1U;
            comm->last_valid_rx_ms = now_ms;
            saturating_increment(&comm->valid_rx_count);
        }
        else
        {
            saturating_increment(&comm->invalid_rx_count);
        }
    }
}

static void schedule_next_tx(comm_t *comm, uint32_t now_ms)
{
    comm->next_tx_ms += COMM_TX_PERIOD_MS;

    /* 메인 루프가 오래 멈췄다면 밀린 패킷을 몰아서 보내지 않는다. */
    if (time_reached(now_ms, comm->next_tx_ms))
    {
        comm->next_tx_ms = now_ms + COMM_TX_PERIOD_MS;
    }
}

static void send_train_packet(comm_t *comm,
                              const train_fsm_outputs_t *outputs,
                              uint32_t now_ms)
{
    train_to_psd_packet_t packet;
    nrf24_result_t result;

    if (!time_reached(now_ms, comm->next_tx_ms) ||
        nrf24_is_tx_busy())
    {
        return;
    }

    packet_build_train_to_psd(&packet,
                              outputs->state,
                              outputs->psd_command,
                              outputs->countdown,
                              outputs->fault_code);
    result = nrf24_start_send((const uint8_t *)&packet, now_ms);
    comm->last_tx_result = result;

    if (result == NRF24_RESULT_OK)
    {
        schedule_next_tx(comm, now_ms);
    }
}

static void send_psd_packet(comm_t *comm,
                            const psd_fsm_outputs_t *outputs,
                            uint32_t now_ms)
{
    psd_to_train_packet_t packet;
    nrf24_result_t result;

    if (!time_reached(now_ms, comm->next_tx_ms) ||
        nrf24_is_tx_busy())
    {
        return;
    }

    packet_build_psd_to_train(&packet,
                              outputs->state,
                              outputs->request,
                              (uint8_t)(outputs->flags |
                                  (comm->ui_normal ? PSD_FLAG_UI_NORMAL : 0U)),
                              outputs->fault_code);
    result = nrf24_start_send((const uint8_t *)&packet, now_ms);
    comm->last_tx_result = result;

    if (result == NRF24_RESULT_OK)
    {
        schedule_next_tx(comm, now_ms);
    }
}

uint8_t comm_init(comm_t *comm, comm_role_t role, uint32_t now_ms)
{
    nrf24_role_t radio_role;

    if ((comm == NULL) ||
        ((role != COMM_ROLE_TRAIN) && (role != COMM_ROLE_PSD)))
    {
        return 0U;
    }

    memset(comm, 0, sizeof(*comm));
    comm->role = role;
    comm->last_valid_rx_ms = now_ms;
    comm->last_packet_check = PACKET_CHECK_NULL;
    comm->last_tx_result = NRF24_RESULT_NO_DATA;

    /* 같은 주기의 동시 송신 충돌을 줄이기 위해 PSD는 반 주기 늦게 시작한다. */
    comm->next_tx_ms = now_ms;
    if (role == COMM_ROLE_PSD)
    {
        comm->next_tx_ms += (COMM_TX_PERIOD_MS / 2U);
    }

    radio_role = (role == COMM_ROLE_TRAIN)
                     ? NRF24_ROLE_TRAIN
                     : NRF24_ROLE_PSD;
    comm->radio_init_result = nrf24_init(radio_role);
    comm->initialized = (uint8_t)(
        comm->radio_init_result == NRF24_RESULT_OK);

    return comm->initialized;
}

void comm_train_update(comm_t *comm,
                       const train_fsm_outputs_t *train_outputs,
                       uint32_t now_ms,
                       train_fsm_inputs_t *train_inputs)
{
    uint8_t timed_out;
    uint8_t local_is_operating;

    if ((comm == NULL) || (train_outputs == NULL) ||
        (train_inputs == NULL) || (comm->role != COMM_ROLE_TRAIN))
    {
        return;
    }

    if (comm->initialized != 0U)
    {
        service_tx_result(comm, now_ms);
        receive_for_train(comm, now_ms);
    }

    timed_out = comm_has_timed_out(comm, now_ms);
    train_inputs->communication_timed_out = timed_out;
    train_inputs->communication_ok = (uint8_t)(
        comm->initialized && comm->has_valid_rx && !timed_out);
    train_inputs->peer_state_valid = (uint8_t)(comm->has_valid_rx && !timed_out);
    if (train_inputs->communication_ok)
        comm->ui_normal = (uint8_t)((comm->last_psd_packet.flags & PSD_FLAG_UI_NORMAL) != 0U);

    if (comm->has_valid_rx != 0U)
    {
        train_inputs->psd_state =
            (psd_state_t)comm->last_psd_packet.psd_state;
        train_inputs->psd_request =
            (psd_request_t)comm->last_psd_packet.request;
        train_inputs->psd_fault_code =
            (fault_code_t)comm->last_psd_packet.fault_code;
        train_inputs->psd_estop_active = (uint8_t)(
            (comm->last_psd_packet.flags & PSD_FLAG_ESTOP_ACTIVE) != 0U);
        train_inputs->psd_closed_confirmed = (uint8_t)(
            (comm->last_psd_packet.flags & PSD_FLAG_CLOSED_CONFIRMED) != 0U);
    }

    local_is_operating = (uint8_t)(
        (train_outputs->state != TRAIN_STATE_INIT) &&
        (train_outputs->state != TRAIN_STATE_READY) &&
        (train_outputs->state != TRAIN_STATE_FAULT) &&
        (train_outputs->state != TRAIN_STATE_ESTOP));
    train_inputs->peer_reboot_detected = (uint8_t)(
        local_is_operating && comm->has_valid_rx &&
        (comm->last_psd_packet.psd_state == (uint8_t)PSD_STATE_INIT));

}

void comm_psd_update(comm_t *comm,
                     const psd_fsm_outputs_t *psd_outputs,
                     uint32_t now_ms,
                     psd_fsm_inputs_t *psd_inputs)
{
    uint8_t timed_out;
    uint8_t local_is_operating;

    if ((comm == NULL) || (psd_outputs == NULL) ||
        (psd_inputs == NULL) || (comm->role != COMM_ROLE_PSD))
    {
        return;
    }

    if (comm->initialized != 0U)
    {
        service_tx_result(comm, now_ms);
        receive_for_psd(comm, now_ms);
    }

    timed_out = comm_has_timed_out(comm, now_ms);
    psd_inputs->communication_timed_out = timed_out;
    psd_inputs->communication_ok = (uint8_t)(
        comm->initialized && comm->has_valid_rx && !timed_out);
    psd_inputs->peer_state_valid = (uint8_t)(comm->has_valid_rx && !timed_out);

    if (comm->has_valid_rx != 0U)
    {
        psd_inputs->train_state =
            (train_state_t)comm->last_train_packet.train_state;
        psd_inputs->train_command =
            (psd_command_t)comm->last_train_packet.psd_command;
        psd_inputs->train_fault_code =
            (fault_code_t)comm->last_train_packet.fault_code;
    }

    local_is_operating = (uint8_t)(
        (psd_outputs->state != PSD_STATE_INIT) &&
        (psd_outputs->state != PSD_STATE_READY) &&
        (psd_outputs->state != PSD_STATE_FAULT) &&
        (psd_outputs->state != PSD_STATE_ESTOP));
    psd_inputs->peer_reboot_detected = (uint8_t)(
        local_is_operating && comm->has_valid_rx &&
        (comm->last_train_packet.train_state == (uint8_t)TRAIN_STATE_INIT));

}

void comm_train_transmit(comm_t *comm, const train_fsm_outputs_t *out, uint32_t now)
{
    if (comm && out && comm->initialized && comm->role == COMM_ROLE_TRAIN)
        send_train_packet(comm, out, now);
}
void comm_psd_transmit(comm_t *comm, const psd_fsm_outputs_t *out, uint32_t now)
{
    if (comm && out && comm->initialized && comm->role == COMM_ROLE_PSD)
        send_psd_packet(comm, out, now);
}
