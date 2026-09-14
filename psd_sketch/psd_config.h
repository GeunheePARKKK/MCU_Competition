#ifndef PSD_CONFIG_H
#define PSD_CONFIG_H
#include <stdint.h>
/* Confirmed passive piezo wiring. Set 1 only for an active buzzer. */
#define PSD_ACTIVE_BUZZER 0
#define PSD_REQUEST_HOLD_MS 500UL
#define PSD_RECOVERY_TIMEOUT_MS 15000UL
#define PSD_START_DEBOUNCE_MS 30UL
#define PSD_KW11_DEBOUNCE_MS 40UL
#define PSD_ESTOP_RELEASE_CONFIRM_MS 50UL
#define PSD_DOOR_OPEN_TIME_MS 1000UL
#define PSD_DOOR_OPEN_TIMEOUT_MS 60000UL
#define PSD_DOOR_CLOSE_TIMEOUT_MS 60000UL
#endif
