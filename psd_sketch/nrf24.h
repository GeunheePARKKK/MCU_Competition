#ifndef NRF24_H
#define NRF24_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#include "radio_config.h"

/* 이 펌웨어가 TRAIN 보드인지 PSD 보드인지 선택한다. */
typedef enum
{
    NRF24_ROLE_TRAIN = 0,
    NRF24_ROLE_PSD = 1
} nrf24_role_t;

/* 드라이버 함수의 실행 결과 */
typedef enum
{
    NRF24_RESULT_OK = 0,
    NRF24_RESULT_BUSY,
    NRF24_RESULT_NO_DATA,
    NRF24_RESULT_TX_SUCCESS,
    NRF24_RESULT_MAX_RETRY,
    NRF24_RESULT_TIMEOUT,
    NRF24_RESULT_BAD_ARGUMENT,
    NRF24_RESULT_NOT_INITIALIZED,
    NRF24_RESULT_REGISTER_VERIFY_FAILED
} nrf24_result_t;

/* SPI와 nRF 레지스터를 설정한 뒤 수신 대기 상태로 들어간다. */
nrf24_result_t nrf24_init(nrf24_role_t role);

/*
 * 6바이트 송신을 시작한다. 완료될 때까지 기다리지 않는다.
 * 완료 여부는 nrf24_update_tx()로 계속 확인한다.
 */
nrf24_result_t nrf24_start_send(
    const uint8_t payload[NRF_PAYLOAD_SIZE],
    uint32_t now_ms);

/* 진행 중인 송신의 성공, 최대 재시도, 시간초과를 확인한다. */
nrf24_result_t nrf24_update_tx(uint32_t now_ms);

/* 수신 FIFO에 패킷이 있으면 정확히 6바이트를 읽는다. */
nrf24_result_t nrf24_receive(
    uint8_t payload[NRF_PAYLOAD_SIZE]);

uint8_t nrf24_is_tx_busy(void);
uint8_t nrf24_read_status(void);


#ifdef __cplusplus
}
#endif

#endif /* NRF24_H */
