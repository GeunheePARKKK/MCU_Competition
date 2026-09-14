#include "spi.h"

#include <avr/io.h>
#include <stddef.h>

#include "radio_config.h"
#include "timebase.h"

/*
 * 두 보드에서 공통으로 사용하는 ATmega328P 하드웨어 SPI 핀
 * MOSI=PB3, MISO=PB4, SCK=PB5, SS=PB2
 */

/* radio_config.h의 목표 속도를 ATmega SPI 분주비로 바꾼다. */
#if NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 2UL)
#define SPI_CLOCK_SPCR_BITS 0U
#define SPI_CLOCK_SPSR_BITS ((uint8_t)(1U << SPI2X))
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 4UL)
#define SPI_CLOCK_SPCR_BITS 0U
#define SPI_CLOCK_SPSR_BITS 0U
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 8UL)
#define SPI_CLOCK_SPCR_BITS ((uint8_t)(1U << SPR0))
#define SPI_CLOCK_SPSR_BITS ((uint8_t)(1U << SPI2X))
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 16UL)
#define SPI_CLOCK_SPCR_BITS ((uint8_t)(1U << SPR0))
#define SPI_CLOCK_SPSR_BITS 0U
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 32UL)
#define SPI_CLOCK_SPCR_BITS ((uint8_t)(1U << SPR1))
#define SPI_CLOCK_SPSR_BITS ((uint8_t)(1U << SPI2X))
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 64UL)
#define SPI_CLOCK_SPCR_BITS ((uint8_t)(1U << SPR1))
#define SPI_CLOCK_SPSR_BITS 0U
#elif NRF_SPI_CLOCK_HZ_INITIAL == (F_CPU / 128UL)
#define SPI_CLOCK_SPCR_BITS ((uint8_t)((1U << SPR1) | (1U << SPR0)))
#define SPI_CLOCK_SPSR_BITS 0U
#else
#error "NRF_SPI_CLOCK_HZ_INITIAL must match an ATmega328P SPI clock divider"
#endif

void spi_init(void)
{
    /* MOSI와 SCK는 출력, MISO는 입력이다. */
    DDRB |= (uint8_t)((1U << PB3) | (1U << PB5));
    DDRB &= (uint8_t)~(1U << PB4);

    /*
     * PB2/SS가 입력 LOW가 되면 하드웨어가 Slave 모드로 바뀔 수 있다.
     * 이 회로에서 PB2는 위쪽 서보 신호이기도 하므로 출력으로만 만들고
     * PORT 값은 여기서 변경하지 않는다.
     */
    DDRB |= (uint8_t)(1U << PB2);

    /* SPI Mode 0, MSB first, Master, 설정된 클록 속도 */
    SPCR = (uint8_t)((1U << SPE) |
                     (1U << MSTR) |
                     SPI_CLOCK_SPCR_BITS);
    SPSR = SPI_CLOCK_SPSR_BITS;
}

uint8_t spi_transfer(uint8_t tx_data)
{
    SPDR = tx_data;

    while ((SPSR & (uint8_t)(1U << SPIF)) == 0U)
    {
        /* 하드웨어 전송 완료를 기다린다. 한 바이트만 기다리므로 매우 짧다. */
    }

    return SPDR;
}

void spi_transfer_buffer(const uint8_t *tx_data,
                         uint8_t *rx_data,
                         uint8_t length)
{
    uint8_t index;
    uint8_t received;

    for (index = 0U; index < length; ++index)
    {
        received = spi_transfer((tx_data != NULL) ? tx_data[index] : 0xFFU);

        if (rx_data != NULL)
        {
            rx_data[index] = received;
        }
    }
}
