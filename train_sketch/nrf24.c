#include "nrf24.h"

#include <avr/io.h>
#include <stddef.h>
#include <string.h>
#include <util/delay.h>

#include "spi.h"
#include "timebase.h"

/* nRF24L01+ 명령 */
#define NRF_CMD_R_REGISTER       UINT8_C(0x00)
#define NRF_CMD_W_REGISTER       UINT8_C(0x20)
#define NRF_CMD_R_RX_PAYLOAD     UINT8_C(0x61)
#define NRF_CMD_W_TX_PAYLOAD     UINT8_C(0xA0)
#define NRF_CMD_FLUSH_TX         UINT8_C(0xE1)
#define NRF_CMD_FLUSH_RX         UINT8_C(0xE2)
#define NRF_CMD_NOP              UINT8_C(0xFF)
#define NRF_REGISTER_MASK        UINT8_C(0x1F)

/* nRF24L01+ 레지스터 */
#define NRF_REG_CONFIG           UINT8_C(0x00)
#define NRF_REG_EN_AA            UINT8_C(0x01)
#define NRF_REG_EN_RXADDR        UINT8_C(0x02)
#define NRF_REG_SETUP_AW         UINT8_C(0x03)
#define NRF_REG_SETUP_RETR       UINT8_C(0x04)
#define NRF_REG_RF_CH            UINT8_C(0x05)
#define NRF_REG_RF_SETUP         UINT8_C(0x06)
#define NRF_REG_STATUS           UINT8_C(0x07)
#define NRF_REG_RX_ADDR_P0       UINT8_C(0x0A)
#define NRF_REG_TX_ADDR          UINT8_C(0x10)
#define NRF_REG_RX_PW_P0         UINT8_C(0x11)
#define NRF_REG_FIFO_STATUS      UINT8_C(0x17)
#define NRF_REG_DYNPD            UINT8_C(0x1C)
#define NRF_REG_FEATURE          UINT8_C(0x1D)

/* CONFIG 비트 */
#define NRF_CONFIG_MASK_RX_DR    ((uint8_t)(1U << 6))
#define NRF_CONFIG_MASK_TX_DS    ((uint8_t)(1U << 5))
#define NRF_CONFIG_MASK_MAX_RT   ((uint8_t)(1U << 4))
#define NRF_CONFIG_EN_CRC        ((uint8_t)(1U << 3))
#define NRF_CONFIG_CRCO          ((uint8_t)(1U << 2))
#define NRF_CONFIG_PWR_UP        ((uint8_t)(1U << 1))
#define NRF_CONFIG_PRIM_RX       ((uint8_t)(1U << 0))

/* STATUS와 FIFO_STATUS 비트 */
#define NRF_STATUS_RX_DR         ((uint8_t)(1U << 6))
#define NRF_STATUS_TX_DS         ((uint8_t)(1U << 5))
#define NRF_STATUS_MAX_RT        ((uint8_t)(1U << 4))
#define NRF_STATUS_CLEAR_ALL     \
    ((uint8_t)(NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT))
#define NRF_FIFO_RX_EMPTY        ((uint8_t)(1U << 0))

/* RF_SETUP 비트 */
#define NRF_RF_DR_LOW            ((uint8_t)(1U << 5))
#define NRF_RF_DR_HIGH           ((uint8_t)(1U << 3))

/* 두 보드에 공통인 무선 제어핀: CE=PD7, CSN=PB0 */
#define NRF_CE_HIGH()            (PORTD |= (uint8_t)(1U << PD7))
#define NRF_CE_LOW()             (PORTD &= (uint8_t)~(1U << PD7))
#define NRF_CSN_HIGH()           (PORTB |= (uint8_t)(1U << PB0))
#define NRF_CSN_LOW()            (PORTB &= (uint8_t)~(1U << PB0))

static uint8_t g_initialized;
static uint8_t g_tx_busy;
static uint32_t g_tx_start_ms;
static uint8_t g_local_address[NRF_ADDRESS_WIDTH];
static uint8_t g_peer_address[NRF_ADDRESS_WIDTH];

static const uint8_t TRAIN_ADDRESS[NRF_ADDRESS_WIDTH] =
    NRF_TRAIN_ADDRESS_INITIALIZER;
static const uint8_t PSD_ADDRESS[NRF_ADDRESS_WIDTH] =
    NRF_PSD_ADDRESS_INITIALIZER;

static uint8_t config_base_value(void)
{
    uint8_t value = (uint8_t)(NRF_CONFIG_MASK_RX_DR |
                              NRF_CONFIG_MASK_TX_DS |
                              NRF_CONFIG_MASK_MAX_RT);

#if NRF_CRC_BYTES > 0U
    value |= NRF_CONFIG_EN_CRC;
#endif
#if NRF_CRC_BYTES == 2U
    value |= NRF_CONFIG_CRCO;
#endif

    return value;
}

static uint8_t command(uint8_t command_byte)
{
    uint8_t status;

    NRF_CSN_LOW();
    status = spi_transfer(command_byte);
    NRF_CSN_HIGH();
    return status;
}

static uint8_t read_register(uint8_t register_address)
{
    uint8_t value;

    NRF_CSN_LOW();
    (void)spi_transfer((uint8_t)(NRF_CMD_R_REGISTER |
                                (register_address & NRF_REGISTER_MASK)));
    value = spi_transfer(NRF_CMD_NOP);
    NRF_CSN_HIGH();
    return value;
}

static void write_register(uint8_t register_address, uint8_t value)
{
    NRF_CSN_LOW();
    (void)spi_transfer((uint8_t)(NRF_CMD_W_REGISTER |
                                (register_address & NRF_REGISTER_MASK)));
    (void)spi_transfer(value);
    NRF_CSN_HIGH();
}

static void write_register_buffer(uint8_t register_address,
                                  const uint8_t *data,
                                  uint8_t length)
{
    NRF_CSN_LOW();
    (void)spi_transfer((uint8_t)(NRF_CMD_W_REGISTER |
                                (register_address & NRF_REGISTER_MASK)));
    spi_transfer_buffer(data, NULL, length);
    NRF_CSN_HIGH();
}

static uint8_t address_width_register_value(void)
{
#if NRF_ADDRESS_WIDTH == 3U
    return 1U;
#elif NRF_ADDRESS_WIDTH == 4U
    return 2U;
#elif NRF_ADDRESS_WIDTH == 5U
    return 3U;
#else
#error "NRF_ADDRESS_WIDTH must be 3, 4, or 5"
#endif
}

static uint8_t retry_register_value(void)
{
#if NRF_AUTO_ACK_ENABLE
    /* ARD 한 단계는 250 us이며 레지스터에는 (단계 수 - 1)을 기록한다. */
    return (uint8_t)((((NRF_RETRY_DELAY_US_INITIAL / 250U) - 1U) << 4) |
                     (NRF_RETRY_COUNT_INITIAL & 0x0FU));
#else
    return 0U;
#endif
}

static uint8_t rf_setup_register_value(void)
{
    uint8_t value = 0U;

#if NRF_DATA_RATE_KBPS_INITIAL == 250U
    value |= NRF_RF_DR_LOW;
#elif NRF_DATA_RATE_KBPS_INITIAL == 1000U
    /* RF_DR_LOW=0, RF_DR_HIGH=0 */
#elif NRF_DATA_RATE_KBPS_INITIAL == 2000U
    value |= NRF_RF_DR_HIGH;
#else
#error "nRF24L01+ data rate must be 250, 1000, or 2000 kbps"
#endif

#if NRF_TX_POWER_DBM_INITIAL == 0
    value |= (uint8_t)(3U << 1);
#elif NRF_TX_POWER_DBM_INITIAL == -6
    value |= (uint8_t)(2U << 1);
#elif NRF_TX_POWER_DBM_INITIAL == -12
    value |= (uint8_t)(1U << 1);
#elif NRF_TX_POWER_DBM_INITIAL == -18
    /* RF_PWR=00 */
#else
#error "nRF24L01+ power must be 0, -6, -12, or -18 dBm"
#endif

    return value;
}

static void clear_interrupt_flags(void)
{
    write_register(NRF_REG_STATUS, NRF_STATUS_CLEAR_ALL);
}

static void enter_receive_mode(uint8_t power_up_delay_needed)
{
    NRF_CE_LOW();
    write_register_buffer(NRF_REG_RX_ADDR_P0,
                          g_local_address,
                          NRF_ADDRESS_WIDTH);
    write_register(NRF_REG_CONFIG,
                   (uint8_t)(config_base_value() |
                             NRF_CONFIG_PWR_UP |
                             NRF_CONFIG_PRIM_RX));

    if (power_up_delay_needed != 0U)
    {
        _delay_ms(2);
    }

    NRF_CE_HIGH();
    _delay_us(NRF_RX_SETTLE_US);
}

nrf24_result_t nrf24_init(nrf24_role_t role)
{
    uint8_t setup_aw;

    if ((role != NRF24_ROLE_TRAIN) && (role != NRF24_ROLE_PSD))
    {
        return NRF24_RESULT_BAD_ARGUMENT;
    }

    g_initialized = 0U;
    g_tx_busy = 0U;

    DDRD |= (uint8_t)(1U << PD7);
    DDRB |= (uint8_t)(1U << PB0);
    NRF_CE_LOW();
    NRF_CSN_HIGH();

    spi_init();
    _delay_ms(5);

    if (role == NRF24_ROLE_TRAIN)
    {
        memcpy(g_local_address, TRAIN_ADDRESS, NRF_ADDRESS_WIDTH);
        memcpy(g_peer_address, PSD_ADDRESS, NRF_ADDRESS_WIDTH);
    }
    else
    {
        memcpy(g_local_address, PSD_ADDRESS, NRF_ADDRESS_WIDTH);
        memcpy(g_peer_address, TRAIN_ADDRESS, NRF_ADDRESS_WIDTH);
    }

    /* 먼저 Power Down 상태에서 모든 공통 레지스터를 설정한다. */
    write_register(NRF_REG_CONFIG, config_base_value());
    write_register(NRF_REG_EN_AA, NRF_AUTO_ACK_ENABLE ? 0x01U : 0x00U);
    write_register(NRF_REG_EN_RXADDR, 0x01U);
    write_register(NRF_REG_SETUP_AW, address_width_register_value());
    write_register(NRF_REG_SETUP_RETR, retry_register_value());
    write_register(NRF_REG_RF_CH, NRF_RF_CHANNEL_INITIAL);
    write_register(NRF_REG_RF_SETUP, rf_setup_register_value());
    write_register(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);
    write_register(NRF_REG_DYNPD, 0x00U);
    write_register(NRF_REG_FEATURE, 0x00U);

    write_register_buffer(NRF_REG_RX_ADDR_P0,
                          g_local_address,
                          NRF_ADDRESS_WIDTH);
    write_register_buffer(NRF_REG_TX_ADDR,
                          g_peer_address,
                          NRF_ADDRESS_WIDTH);

    clear_interrupt_flags();
    (void)command(NRF_CMD_FLUSH_RX);
    (void)command(NRF_CMD_FLUSH_TX);

    /* 가장 중요한 설정값을 다시 읽어 배선/SPI 초기화 실패를 빠르게 찾는다. */
    setup_aw = read_register(NRF_REG_SETUP_AW);
    if ((setup_aw != address_width_register_value()) ||
        (read_register(NRF_REG_RF_CH) != NRF_RF_CHANNEL_INITIAL) ||
        (read_register(NRF_REG_RX_PW_P0) != NRF_PAYLOAD_SIZE))
    {
        return NRF24_RESULT_REGISTER_VERIFY_FAILED;
    }

    enter_receive_mode(1U);
    g_initialized = 1U;
    return NRF24_RESULT_OK;
}

nrf24_result_t nrf24_start_send(
    const uint8_t payload[NRF_PAYLOAD_SIZE],
    uint32_t now_ms)
{
    if (payload == NULL)
    {
        return NRF24_RESULT_BAD_ARGUMENT;
    }
    if (g_initialized == 0U)
    {
        return NRF24_RESULT_NOT_INITIALIZED;
    }
    if (g_tx_busy != 0U)
    {
        return NRF24_RESULT_BUSY;
    }

    NRF_CE_LOW();

    /* Auto ACK를 받기 위해 TX_ADDR와 RX_ADDR_P0을 상대 주소로 맞춘다. */
    write_register_buffer(NRF_REG_TX_ADDR,
                          g_peer_address,
                          NRF_ADDRESS_WIDTH);
    write_register_buffer(NRF_REG_RX_ADDR_P0,
                          g_peer_address,
                          NRF_ADDRESS_WIDTH);
    write_register(NRF_REG_CONFIG,
                   (uint8_t)(config_base_value() | NRF_CONFIG_PWR_UP));

    clear_interrupt_flags();
    (void)command(NRF_CMD_FLUSH_TX);

    NRF_CSN_LOW();
    (void)spi_transfer(NRF_CMD_W_TX_PAYLOAD);
    spi_transfer_buffer(payload, NULL, NRF_PAYLOAD_SIZE);
    NRF_CSN_HIGH();

    /* CE를 10 us 이상 HIGH로 만들면 실제 무선 송신이 시작된다. */
    NRF_CE_HIGH();
    _delay_us(NRF_CE_PULSE_US);
    NRF_CE_LOW();

    g_tx_start_ms = now_ms;
    g_tx_busy = 1U;
    return NRF24_RESULT_OK;
}

nrf24_result_t nrf24_update_tx(uint32_t now_ms)
{
    uint8_t status;

    if (g_initialized == 0U)
    {
        return NRF24_RESULT_NOT_INITIALIZED;
    }
    if (g_tx_busy == 0U)
    {
        return NRF24_RESULT_NO_DATA;
    }

    status = nrf24_read_status();

    if ((status & NRF_STATUS_TX_DS) != 0U)
    {
        write_register(NRF_REG_STATUS, NRF_STATUS_TX_DS);
        g_tx_busy = 0U;
        enter_receive_mode(0U);
        return NRF24_RESULT_TX_SUCCESS;
    }

    if ((status & NRF_STATUS_MAX_RT) != 0U)
    {
        write_register(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
        (void)command(NRF_CMD_FLUSH_TX);
        g_tx_busy = 0U;
        enter_receive_mode(0U);
        return NRF24_RESULT_MAX_RETRY;
    }

    if (timebase_has_elapsed(now_ms,
                             g_tx_start_ms,
                             NRF_TX_RESULT_TIMEOUT_MS))
    {
        clear_interrupt_flags();
        (void)command(NRF_CMD_FLUSH_TX);
        g_tx_busy = 0U;
        enter_receive_mode(0U);
        return NRF24_RESULT_TIMEOUT;
    }

    return NRF24_RESULT_BUSY;
}

nrf24_result_t nrf24_receive(uint8_t payload[NRF_PAYLOAD_SIZE])
{
    if (payload == NULL)
    {
        return NRF24_RESULT_BAD_ARGUMENT;
    }
    if (g_initialized == 0U)
    {
        return NRF24_RESULT_NOT_INITIALIZED;
    }
    if (g_tx_busy != 0U)
    {
        return NRF24_RESULT_BUSY;
    }
    if ((read_register(NRF_REG_FIFO_STATUS) & NRF_FIFO_RX_EMPTY) != 0U)
    {
        return NRF24_RESULT_NO_DATA;
    }

    NRF_CSN_LOW();
    (void)spi_transfer(NRF_CMD_R_RX_PAYLOAD);
    spi_transfer_buffer(NULL, payload, NRF_PAYLOAD_SIZE);
    NRF_CSN_HIGH();

    write_register(NRF_REG_STATUS, NRF_STATUS_RX_DR);
    return NRF24_RESULT_OK;
}

uint8_t nrf24_is_tx_busy(void)
{
    return g_tx_busy;
}

uint8_t nrf24_read_status(void)
{
    return command(NRF_CMD_NOP);
}
