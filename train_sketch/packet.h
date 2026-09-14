#ifndef PACKET_H
#define PACKET_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "protocol.h"
typedef enum
{
    PACKET_CHECK_OK = 0,
    PACKET_CHECK_NULL,
    PACKET_CHECK_BAD_MAGIC,
    PACKET_CHECK_BAD_VERSION,
    PACKET_CHECK_BAD_TRAIN_STATE,
    PACKET_CHECK_BAD_PSD_STATE,
    PACKET_CHECK_BAD_COMMAND,
    PACKET_CHECK_BAD_REQUEST,
    PACKET_CHECK_BAD_COUNTDOWN,
    PACKET_CHECK_BAD_FLAGS,
    PACKET_CHECK_BAD_FAULT_CODE,
    PACKET_CHECK_COMMAND_STATE_MISMATCH,
    PACKET_CHECK_REQUEST_STATE_MISMATCH,
    PACKET_CHECK_COUNTDOWN_STATE_MISMATCH,
    PACKET_CHECK_ESTOP_STATE_MISMATCH,
    PACKET_CHECK_FAULT_STATE_MISMATCH
} packet_check_result_t;
void packet_build_train_to_psd(train_to_psd_packet_t *packet, train_state_t train_state, psd_command_t psd_command, uint8_t countdown, fault_code_t fault_code);
void packet_build_psd_to_train(psd_to_train_packet_t *packet, psd_state_t psd_state, psd_request_t request, uint8_t flags, fault_code_t fault_code);
packet_check_result_t packet_validate_train_to_psd(const train_to_psd_packet_t *packet);
packet_check_result_t packet_validate_psd_to_train(const psd_to_train_packet_t *packet);
uint8_t packet_train_to_psd_is_valid(const train_to_psd_packet_t *packet);
uint8_t packet_psd_to_train_is_valid(const psd_to_train_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
