#include "door_stepper.h"

#include "psd_config.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <util/delay.h>

/* 칩 물리핀 기준. psd_sketch의 다른 핀과 겹치지 않는다. */
#define STEP_BIT  PB1 /* 15번 D9 */
#define DIR_BIT   PC3 /* 26번 A3 */
#define EN_BIT    PD0 /* 2번 D0, LOW=구동. 외부 10k 풀업으로 리셋·업로드 중엔 꺼져 있다 */
#define ESTOP_BIT PD2 /* 4번 D2, LOW=눌림. psd_sketch의 readEstop()과 같은 버튼 */

/* Timer1을 /8 분주로 쓰면 1틱 = 0.5us */
#define US_TO_TICKS(us) ((uint16_t)((uint32_t)(us) * 2UL - 1UL))
#define RAMP_DELTA_US \
    ((uint16_t)((PSD_STEPPER_START_INTERVAL_US - PSD_STEPPER_RUN_INTERVAL_US) / PSD_STEPPER_RAMP_STEPS))

static volatile int16_t s_position; /* 0 = 닫힘, + 방향 = 열림 */
static volatile int16_t s_target;
static volatile int8_t s_dir;       /* DIR 핀이 지금 가리키는 방향. 0 = 아직 정하지 않음 */
static volatile uint8_t s_running;
static volatile uint8_t s_ramp_n;   /* 출발 후 걸음 수. 가속 계산용 */
static uint32_t s_idle_since_ms;

static inline uint8_t estop_pressed(void)
{
    return (uint8_t)((PIND & (1U << ESTOP_BIT)) == 0U);
}

static inline void driver_enable(void)
{
    PORTD &= (uint8_t)~(1U << EN_BIT);
}

static inline void driver_disable(void)
{
    PORTD |= (uint8_t)(1U << EN_BIT);
}

static void set_dir_pin(int8_t dir)
{
    uint8_t high = (dir > 0) ? (uint8_t)PSD_STEPPER_OPEN_DIR_HIGH
                             : (uint8_t)!PSD_STEPPER_OPEN_DIR_HIGH;
    if (high)
        PORTC |= (uint8_t)(1U << DIR_BIT);
    else
        PORTC &= (uint8_t)~(1U << DIR_BIT);
}

/* 인터럽트가 꺼진 상태나 ISR 안에서만 부른다. */
static void timer_stop(void)
{
    TIMSK1 &= (uint8_t)~(1U << OCIE1A);
    TCCR1B = 0U;
    s_running = 0U;
}

static void timer_start(void)
{
    TCCR1B = 0U;
    TCCR1A = 0U;
    TCNT1 = 0U;
    /* 첫 걸음은 느린 간격 뒤에 나간다. EN을 켠 직후 코일이 자리 잡을 시간이기도 하다. */
    OCR1A = US_TO_TICKS(PSD_STEPPER_START_INTERVAL_US);
    TIFR1 = (uint8_t)(1U << OCF1A);
    TIMSK1 |= (uint8_t)(1U << OCIE1A);
    s_running = 1U;
    TCCR1B = (uint8_t)((1U << WGM12) | (1U << CS11)); /* CTC, /8 */
}

/* 출발 직후와 도착 직전은 느리게, 가운데는 최고 속도로. */
static uint16_t next_interval_us(void)
{
    int16_t left = (int16_t)(s_target - s_position);
    uint16_t n = s_ramp_n;

    if (left < 0)
        left = (int16_t)-left;
    if ((uint16_t)left < n)
        n = (uint16_t)left;
    if (n >= PSD_STEPPER_RAMP_STEPS)
        return (uint16_t)PSD_STEPPER_RUN_INTERVAL_US;
    return (uint16_t)(PSD_STEPPER_START_INTERVAL_US - n * RAMP_DELTA_US);
}

ISR(TIMER1_COMPA_vect)
{
    int16_t remaining;
    int8_t want;

    /* loop()가 LCD 갱신 중이어도 비상정지는 여기서 바로 처리한다. */
    if (estop_pressed())
    {
        driver_disable();
        timer_stop();
        s_target = s_position;
        return;
    }

    remaining = (int16_t)(s_target - s_position);
    if (remaining == 0)
    {
        timer_stop();
        return;
    }

    want = (remaining > 0) ? 1 : -1;
    if (want != s_dir)
    {
        /* 방향 전환: DIR만 바꾸고 한 박자 쉰 뒤 느린 속도부터 다시 출발한다. */
        s_dir = want;
        set_dir_pin(want);
        s_ramp_n = 0U;
        OCR1A = US_TO_TICKS(PSD_STEPPER_START_INTERVAL_US);
        return;
    }

    PORTB |= (uint8_t)(1U << STEP_BIT);
    _delay_us(3); /* DRV8825 최소 1.9us */
    PORTB &= (uint8_t)~(1U << STEP_BIT);
    s_position = (int16_t)(s_position + want);

    if (s_position == s_target)
    {
        timer_stop();
        return;
    }
    if (s_ramp_n < PSD_STEPPER_RAMP_STEPS)
        s_ramp_n++;
    OCR1A = US_TO_TICKS(next_interval_us());
}

static void request_target(int16_t target)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_target = target;
        if (!s_running && (s_position != target) && !estop_pressed())
        {
            driver_enable();
            s_ramp_n = 0U;
            timer_start();
        }
    }
}

static void stop_here(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        timer_stop();
        s_target = s_position;
    }
}

void door_stepper_init(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        driver_disable(); /* 출력으로 바꾸기 전에 HIGH부터 */
        DDRD |= (uint8_t)(1U << EN_BIT);
        PORTB &= (uint8_t)~(1U << STEP_BIT);
        DDRB |= (uint8_t)(1U << STEP_BIT);
        PORTC &= (uint8_t)~(1U << DIR_BIT);
        DDRC |= (uint8_t)(1U << DIR_BIT);
        DDRD &= (uint8_t)~(1U << ESTOP_BIT); /* 비상정지 입력 + 내부 풀업 */
        PORTD |= (uint8_t)(1U << ESTOP_BIT);

        timer_stop();
        TCCR1A = 0U; /* Arduino init()이 걸어둔 Timer1 PWM 설정 해제 */
        s_position = 0;
        s_target = 0;
        s_dir = 0;
        s_ramp_n = 0U;
    }
    s_idle_since_ms = 0U;
}

void door_stepper_update(psd_door_command_t command, uint32_t now_ms)
{
#if PSD_STEPPER_ENABLE
    if ((command == PSD_DOOR_RELEASE) || estop_pressed())
    {
        stop_here();
        driver_disable();
        return;
    }

    if (command == PSD_DOOR_OPEN)
        request_target((int16_t)PSD_STEPPER_OPEN_STEPS);
    else if (command == PSD_DOOR_CLOSE)
        request_target(0);
    else
        stop_here(); /* HOLD: 그 자리에 멈춘다 */

    if (s_running)
        s_idle_since_ms = now_ms;
    else if (door_stepper_is_energized() &&
             (uint32_t)(now_ms - s_idle_since_ms) >= PSD_STEPPER_IDLE_RELEASE_MS)
        driver_disable();
#else
    (void)command;
    (void)now_ms;
    stop_here();
    driver_disable();
#endif
}

int16_t door_stepper_position(void)
{
    int16_t position;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        position = s_position;
    }
    return position;
}

uint8_t door_stepper_is_moving(void)
{
    return s_running;
}

uint8_t door_stepper_is_energized(void)
{
    return (uint8_t)((PORTD & (1U << EN_BIT)) == 0U);
}
