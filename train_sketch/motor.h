#ifndef MOTOR_H
#define MOTOR_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

/*
 * DC 모터(IRLZ44N 게이트 구동) 드라이버.
 * PD3(OC2B) / Timer2 Fast PWM, 8비트 듀티(0~255).
 *
 * train_fsm.h의 motor_command_t와 값이 같은 순서로 정의되어 있으므로
 * (STOP=0, RUN=1, APPROACH=2) 호출부에서 (uint8_t)로 캐스팅해서 넘기면 된다.
 */

#define MOTOR_CMD_STOP     UINT8_C(0)
#define MOTOR_CMD_RUN      UINT8_C(1)
#define MOTOR_CMD_APPROACH UINT8_C(2)

/* run_pwm / approach_pwm : train_config.h의 PWM 더미값을 그대로 받는다. */
typedef struct
{
    uint8_t run_pwm;
    uint8_t approach_pwm;
} motor_config_t;

/* Timer2를 976Hz(16MHz/64/256) Fast PWM으로 초기화하고 PD3을 출력으로 설정한다. */
void motor_init(void);

/*
 * command: MOTOR_CMD_STOP / RUN / APPROACH 중 하나.
 * STOP이면 듀티 0 (즉시 정지, 관성으로 서서히 멈춤 - 급제동 아님).
 */
void motor_apply_command(uint8_t command, const motor_config_t *config);


#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H */
