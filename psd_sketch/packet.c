#include "packet.h"

#include <stddef.h>

static uint8_t value_is_in_range(uint8_t value, uint8_t count)
{
    return (uint8_t)(value < count);
}

void packet_build_train_to_psd(train_to_psd_packet_t *packet,
                               train_state_t train_state,
                               psd_command_t psd_command,
                               uint8_t countdown,
                               fault_code_t fault_code)
{
    if (packet == NULL)
    {
        return;
    }

    packet->magic = PROTOCOL_MAGIC;
    packet->version = PROTOCOL_VERSION;
    packet->train_state = (uint8_t)train_state;
    packet->psd_command = (uint8_t)psd_command;
    packet->countdown = countdown;
    packet->fault_code = (uint8_t)fault_code;
}

void packet_build_psd_to_train(psd_to_train_packet_t *packet,
                               psd_state_t psd_state,
                               psd_request_t request,
                               uint8_t flags,
                               fault_code_t fault_code)
{
    if (packet == NULL)
    {
        return;
    }

    packet->magic = PROTOCOL_MAGIC;
    packet->version = PROTOCOL_VERSION;
    packet->psd_state = (uint8_t)psd_state;
    packet->request = (uint8_t)request;
    packet->flags = flags;
    packet->fault_code = (uint8_t)fault_code;
}

static packet_check_result_t validate_common(uint8_t magic,
                                             uint8_t version,
                                             uint8_t fault_code)
{
    if (magic != PROTOCOL_MAGIC)
    {
        return PACKET_CHECK_BAD_MAGIC;
    }

    if (version != PROTOCOL_VERSION)
    {
        return PACKET_CHECK_BAD_VERSION;
    }

    if (!value_is_in_range(fault_code, FAULT_CODE_COUNT))
    {
        return PACKET_CHECK_BAD_FAULT_CODE;
    }

    return PACKET_CHECK_OK;
}

static packet_check_result_t validate_train_command_state(
    const train_to_psd_packet_t *packet)
{
    psd_command_t expected_command = PSD_CMD_NONE;

    if (packet->train_state == (uint8_t)TRAIN_STATE_WAIT_PSD_OPEN)
    {
        expected_command = PSD_CMD_OPEN;
    }
    else if (packet->train_state == (uint8_t)TRAIN_STATE_WAIT_PSD_CLOSED)
    {
        expected_command = PSD_CMD_CLOSE;
    }

    if (packet->psd_command != (uint8_t)expected_command)
    {
        return PACKET_CHECK_COMMAND_STATE_MISMATCH;
    }

    return PACKET_CHECK_OK;
}

static packet_check_result_t validate_train_countdown_state(
    const train_to_psd_packet_t *packet)
{
    if (packet->train_state == (uint8_t)TRAIN_STATE_COUNTDOWN)
    {
        if (packet->countdown > COUNTDOWN_MAX_SECONDS)
        {
            return PACKET_CHECK_BAD_COUNTDOWN;
        }
    }
    else if (packet->countdown != COUNTDOWN_INACTIVE)
    {
        return PACKET_CHECK_COUNTDOWN_STATE_MISMATCH;
    }

    return PACKET_CHECK_OK;
}

static packet_check_result_t validate_train_fault_state(
    const train_to_psd_packet_t *packet)
{
    if (packet->train_state == (uint8_t)TRAIN_STATE_ESTOP)
    {
        if (packet->fault_code != (uint8_t)FAULT_CODE_ESTOP)
        {
            return PACKET_CHECK_FAULT_STATE_MISMATCH;
        }
    }
    else if (packet->train_state == (uint8_t)TRAIN_STATE_FAULT)
    {
        if ((packet->fault_code == (uint8_t)FAULT_CODE_NONE) ||
            (packet->fault_code == (uint8_t)FAULT_CODE_ESTOP))
        {
            return PACKET_CHECK_FAULT_STATE_MISMATCH;
        }
    }
    else if (packet->fault_code != (uint8_t)FAULT_CODE_NONE)
    {
        return PACKET_CHECK_FAULT_STATE_MISMATCH;
    }

    return PACKET_CHECK_OK;
}

packet_check_result_t packet_validate_train_to_psd(
    const train_to_psd_packet_t *packet)
{
    packet_check_result_t result;

    if (packet == NULL)
    {
        return PACKET_CHECK_NULL;
    }

    result = validate_common(packet->magic,
                             packet->version,
                             packet->fault_code);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    if (!value_is_in_range(packet->train_state, TRAIN_STATE_COUNT))
    {
        return PACKET_CHECK_BAD_TRAIN_STATE;
    }

    if (!value_is_in_range(packet->psd_command, PSD_COMMAND_COUNT))
    {
        return PACKET_CHECK_BAD_COMMAND;
    }

    result = validate_train_command_state(packet);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    result = validate_train_countdown_state(packet);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    return validate_train_fault_state(packet);
}

static packet_check_result_t validate_psd_request_state(
    const psd_to_train_packet_t *packet)
{
    if ((packet->request == (uint8_t)PSD_REQUEST_START) &&
        (packet->psd_state != (uint8_t)PSD_STATE_READY))
    {
        return PACKET_CHECK_REQUEST_STATE_MISMATCH;
    }

    if (packet->request == (uint8_t)PSD_REQUEST_RECOVERY)
    {
        if ((packet->psd_state != (uint8_t)PSD_STATE_INIT) &&
            (packet->psd_state != (uint8_t)PSD_STATE_FAULT) &&
            (packet->psd_state != (uint8_t)PSD_STATE_ESTOP))
        {
            return PACKET_CHECK_REQUEST_STATE_MISMATCH;
        }
    }

    return PACKET_CHECK_OK;
}

static packet_check_result_t validate_psd_estop_state(
    const psd_to_train_packet_t *packet)
{
    uint8_t estop_flag =
        (uint8_t)(packet->flags & PSD_FLAG_ESTOP_ACTIVE);

    if (packet->psd_state == (uint8_t)PSD_STATE_ESTOP)
    {
        if (estop_flag == 0U)
        {
            return PACKET_CHECK_ESTOP_STATE_MISMATCH;
        }
    }
    else if (estop_flag != 0U)
    {
        return PACKET_CHECK_ESTOP_STATE_MISMATCH;
    }

    return PACKET_CHECK_OK;
}

static packet_check_result_t validate_psd_fault_state(
    const psd_to_train_packet_t *packet)
{
    if (packet->psd_state == (uint8_t)PSD_STATE_ESTOP)
    {
        if (packet->fault_code != (uint8_t)FAULT_CODE_ESTOP)
        {
            return PACKET_CHECK_FAULT_STATE_MISMATCH;
        }
    }
    else if (packet->psd_state == (uint8_t)PSD_STATE_FAULT)
    {
        if ((packet->fault_code == (uint8_t)FAULT_CODE_NONE) ||
            (packet->fault_code == (uint8_t)FAULT_CODE_ESTOP))
        {
            return PACKET_CHECK_FAULT_STATE_MISMATCH;
        }
    }
    else if (packet->fault_code != (uint8_t)FAULT_CODE_NONE)
    {
        return PACKET_CHECK_FAULT_STATE_MISMATCH;
    }

    return PACKET_CHECK_OK;
}

packet_check_result_t packet_validate_psd_to_train(
    const psd_to_train_packet_t *packet)
{
    packet_check_result_t result;

    if (packet == NULL)
    {
        return PACKET_CHECK_NULL;
    }

    result = validate_common(packet->magic,
                             packet->version,
                             packet->fault_code);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    if (!value_is_in_range(packet->psd_state, PSD_STATE_COUNT))
    {
        return PACKET_CHECK_BAD_PSD_STATE;
    }

    if (!value_is_in_range(packet->request, PSD_REQUEST_COUNT))
    {
        return PACKET_CHECK_BAD_REQUEST;
    }

    if ((packet->flags & (uint8_t)(~PSD_FLAGS_VALID_MASK)) != 0U)
    {
        return PACKET_CHECK_BAD_FLAGS;
    }

    result = validate_psd_request_state(packet);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    result = validate_psd_estop_state(packet);
    if (result != PACKET_CHECK_OK)
    {
        return result;
    }

    return validate_psd_fault_state(packet);
}

uint8_t packet_train_to_psd_is_valid(
    const train_to_psd_packet_t *packet)
{
    return (uint8_t)(
        packet_validate_train_to_psd(packet) == PACKET_CHECK_OK);
}

uint8_t packet_psd_to_train_is_valid(
    const psd_to_train_packet_t *packet)
{
    return (uint8_t)(
        packet_validate_psd_to_train(packet) == PACKET_CHECK_OK);
}
