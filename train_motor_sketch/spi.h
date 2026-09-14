#ifndef SPI_H
#define SPI_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

/*
 * ATmega328P의 하드웨어 SPI를 Master 모드로 초기화한다.
 * 현재 프로젝트에서는 nRF24L01+ 전용 SPI로 사용한다.
 */
void spi_init(void);

/*
 * 한 바이트를 보내는 동시에 한 바이트를 받는다.
 * SPI는 송신과 수신이 항상 동시에 일어나므로 반환값이 수신 바이트다.
 */
uint8_t spi_transfer(uint8_t tx_data);

/*
 * 여러 바이트를 연속 전송한다.
 * tx_data가 NULL이면 0xFF를 보내고,
 * rx_data가 NULL이면 수신 바이트를 버린다.
 */
void spi_transfer_buffer(const uint8_t *tx_data,
                         uint8_t *rx_data,
                         uint8_t length);


#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
