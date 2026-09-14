#ifndef COMM_H
#define COMM_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#include "nrf24.h"
#include "packet.h"
#include "psd_fsm.h"
#include "train_fsm.h"

typedef enum
{
    COMM_ROLE_TRAIN = 0,
    COMM_ROLE_PSD = 1
} comm_role_t;

/*
 * 통신 계층이 기억해야 하는 값.
 * IRQ/ISR에서 수정하지 않고 메인 루프에서만 사용하므로 volatile은 필요 없다.
 */
typedef struct
{
    comm_role_t role;
    uint8_t initialized;
    uint8_t has_valid_rx;
    uint8_t ui_normal; /* PSD owns it; TRAIN mirrors only a fresh valid packet. */
    uint32_t last_valid_rx_ms;
    uint32_t next_tx_ms;

    train_to_psd_packet_t last_train_packet;
    psd_to_train_packet_t last_psd_packet;

    packet_check_result_t last_packet_check;
    nrf24_result_t radio_init_result;
    nrf24_result_t last_tx_result;

    uint16_t valid_rx_count;
    uint16_t invalid_rx_count;
    uint16_t tx_success_count;
    uint16_t tx_failure_count;
} comm_t;

/* nRF까지 초기화한다. TRAIN/PSD에서 각각 한 번 호출한다. */
uint8_t comm_init(comm_t *comm, comm_role_t role, uint32_t now_ms);

/* Receive before FSM update; transmit the NEW outputs afterwards. */
void comm_train_update(comm_t *comm,
                       const train_fsm_outputs_t *train_outputs,
                       uint32_t now_ms,
                       train_fsm_inputs_t *train_inputs);

/* PSD 메인 루프에서 매번 호출한다. */
void comm_psd_update(comm_t *comm,
                     const psd_fsm_outputs_t *psd_outputs,
                     uint32_t now_ms,
                     psd_fsm_inputs_t *psd_inputs);


void comm_train_transmit(comm_t *comm, const train_fsm_outputs_t *out, uint32_t now);
void comm_psd_transmit(comm_t *comm, const psd_fsm_outputs_t *out, uint32_t now);
#ifdef __cplusplus
}
#endif

#endif /* COMM_H */
