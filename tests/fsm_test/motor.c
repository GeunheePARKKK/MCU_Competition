#include "motor.h"

#include <avr/io.h>
#include <stddef.h>

void motor_init(void)
{
    DDRD |= (uint8_t)(1U << PD3);

    /* COM2B1=1: OC2B 비반전 PWM. WGM21+WGM20: Fast PWM, TOP=0xFF(Mode 3). */
    TCCR2A = (uint8_t)((1U << WGM21) | (1U << WGM20));
    PORTD &= (uint8_t)~(1U << PD3);

    /* CS22=1: 프리스케일러 64 -> 16MHz/64/256 = 약 976Hz */
    TCCR2B = (uint8_t)(1U << CS22);

    OCR2B = 0U; /* 시작은 정지 상태 */
}

void motor_apply_command(uint8_t command, const motor_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    switch (command)
    {
        case MOTOR_CMD_RUN:
            OCR2B = config->run_pwm;
            TCCR2A |= (uint8_t)(1U << COM2B1);
            break;

        case MOTOR_CMD_APPROACH:
            OCR2B = config->approach_pwm;
            TCCR2A |= (uint8_t)(1U << COM2B1);
            break;

        case MOTOR_CMD_STOP:
        default:
            OCR2B = 0U;
            TCCR2A &= (uint8_t)~((1U << COM2B1) | (1U << COM2B0));
            PORTD &= (uint8_t)~(1U << PD3);
            break;
    }
}
