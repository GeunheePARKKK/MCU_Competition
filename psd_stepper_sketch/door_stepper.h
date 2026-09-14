#ifndef DOOR_STEPPER_H
#define DOOR_STEPPER_H

#include <stdint.h>

#include "psd_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PSD 문 스텝모터 (DRV8825 + BJ42D15, 풀스텝).
 * 칩 물리핀: STEP=15번(D9/PB1), DIR=26번(A3/PC3), EN=2번(D0/PD0, LOW=구동, 외부 10k 풀업).
 *
 * 위치 0 = 닫힘. 원점 센서가 없으므로 전원을 켤 때 문이 닫힌 위치에 있다고 가정한다.
 * 걸음 펄스는 Timer1 인터럽트가 만든다. LCD 갱신이 loop()를 붙잡아도 속도가 흔들리지 않는다.
 * 비상정지(4번, D2)는 인터럽트 안에서도 직접 읽어 loop()를 기다리지 않고 즉시 멈추고 EN을 끊는다.
 */
void door_stepper_init(void);

/* FSM의 문 명령을 매 loop()마다 넘긴다. OPEN/CLOSE=이동, HOLD=그 자리 정지, RELEASE=정지+코일 차단. */
void door_stepper_update(psd_door_command_t command, uint32_t now_ms);

int16_t door_stepper_position(void);
uint8_t door_stepper_is_moving(void);
uint8_t door_stepper_is_energized(void);

#ifdef __cplusplus
}
#endif

#endif /* DOOR_STEPPER_H */
