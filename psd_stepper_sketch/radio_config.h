#ifndef RADIO_CONFIG_H
#define RADIO_CONFIG_H

#include <stdint.h>

#include "protocol.h"

/* 실제 TRAIN-PSD 송수신 시험이 끝나면 1로 변경한다. */
#define RADIO_CONFIG_TESTED                    UINT8_C(0)

/* nRF24L01+ 공통 무선 설정 더미값 */
#define NRF_RF_CHANNEL_INITIAL                 UINT8_C(76)
#define NRF_ADDRESS_WIDTH                      UINT8_C(5)
#define NRF_DATA_RATE_KBPS_INITIAL             UINT16_C(1000)
#define NRF_TX_POWER_DBM_INITIAL               0
#define NRF_CRC_BYTES                          UINT8_C(2)
#define NRF_AUTO_ACK_ENABLE                    UINT8_C(1)
#define NRF_DYNAMIC_PAYLOAD_ENABLE             UINT8_C(0)
#define NRF_RETRY_DELAY_US_INITIAL             UINT16_C(500)
#define NRF_RETRY_COUNT_INITIAL                UINT8_C(5)
#define NRF_SPI_CLOCK_HZ_INITIAL               UINT32_C(1000000)
#define NRF_CE_PULSE_US                        UINT8_C(15)
#define NRF_RX_SETTLE_US                       UINT16_C(150)
#define NRF_TX_RESULT_TIMEOUT_MS               UINT32_C(20)

/* 페이로드 크기는 protocol.h의 6바이트 정의를 그대로 사용한다. */
#define NRF_PAYLOAD_SIZE                       PROTOCOL_PAYLOAD_SIZE

/*
 * 5바이트 주소 더미값.
 * TRAIN RX = "TRN01", PSD RX = "PSD01"
 */
#define NRF_TRAIN_ADDRESS_INITIALIZER          \
    {UINT8_C(0x54), UINT8_C(0x52), UINT8_C(0x4E), \
     UINT8_C(0x30), UINT8_C(0x31)}

#define NRF_PSD_ADDRESS_INITIALIZER            \
    {UINT8_C(0x50), UINT8_C(0x53), UINT8_C(0x44), \
     UINT8_C(0x30), UINT8_C(0x31)}

/* 통신 상위 시간 설정 */
#define COMM_TX_PERIOD_MS                      UINT32_C(50)
#define COMM_TIMEOUT_MS                        UINT32_C(2000)

/* 범위를 벗어난 더미값을 실수로 넣지 못하게 컴파일 단계에서 검사 */
#if NRF_RF_CHANNEL_INITIAL > 125U
#error "nRF24L01+ RF channel must be 0..125"
#endif

#if NRF_RETRY_COUNT_INITIAL > 15U
#error "nRF24L01+ retry count must be 0..15"
#endif

#if (NRF_RETRY_DELAY_US_INITIAL < 250U) || \
    (NRF_RETRY_DELAY_US_INITIAL > 4000U) || \
    ((NRF_RETRY_DELAY_US_INITIAL % 250U) != 0U)
#error "nRF retry delay must be 250..4000 us in 250 us steps"
#endif

#if (NRF_CRC_BYTES != 1U) && (NRF_CRC_BYTES != 2U)
#error "This project requires a 1-byte or 2-byte nRF CRC"
#endif

#endif /* RADIO_CONFIG_H */