#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * TRAIN <-> PSD 무선 프로토콜 공통 정의.
 *
 * 이 파일은 packet.h/.c, radio_config.h, train_fsm.h/.c, psd_fsm.h/.c,
 * comm.h/.c, nrf24.h/.c 전부가 참조하는 최하위 공통 정의다.
 * 여기 있는 상태·명령·요청 enum의 "순서와 값"은 곧 무선으로 나가는
 * 실제 바이트값이므로, 값을 바꾸면 TRAIN/PSD 양쪽 펌웨어를 반드시
 * 같이 다시 빌드해야 한다.
 */

/* ------------------------------------------------------------------ */
/* 패킷 헤더 상수                                                      */
/* ------------------------------------------------------------------ */

/* 페이로드가 진짜 이 프로토콜의 패킷인지 1차로 걸러내는 식별 바이트 */
#define PROTOCOL_MAGIC              UINT8_C(0xA5)

/* 프로토콜(패킷 레이아웃) 버전. 필드를 추가/변경하면 올린다. */
/* v4: virtual-door / motors-OFF packets with a PSD-owned LCD mode bit. */
#define PROTOCOL_VERSION            UINT8_C(4)

/* nRF24L01+ 고정 페이로드 크기. radio_config.h가 NRF_PAYLOAD_SIZE로 그대로 가져다 씀. */
#define PROTOCOL_PAYLOAD_SIZE       UINT8_C(6)

/* ------------------------------------------------------------------ */
/* TRAIN 상태머신                                                      */
/* ------------------------------------------------------------------ */

typedef enum
{
    TRAIN_STATE_INIT = 0,
    TRAIN_STATE_READY,
    TRAIN_STATE_RUNNING,
    TRAIN_STATE_APPROACH,
    TRAIN_STATE_COASTING,
    TRAIN_STATE_WAIT_PSD_OPEN,
    TRAIN_STATE_DOOR_OPENING,
    TRAIN_STATE_DWELL,
    TRAIN_STATE_COUNTDOWN,
    TRAIN_STATE_DOOR_CLOSING,
    TRAIN_STATE_WAIT_PSD_CLOSED,
    TRAIN_STATE_COMPLETE,
    TRAIN_STATE_FAULT,
    TRAIN_STATE_ESTOP,
    TRAIN_STATE_COUNT   /* 실제 상태 아님. 범위검사용 개수. */
} train_state_t;

/* ------------------------------------------------------------------ */
/* PSD 상태머신                                                        */
/* ------------------------------------------------------------------ */

typedef enum
{
    PSD_STATE_INIT = 0,
    PSD_STATE_READY,
    PSD_STATE_OPENING,
    PSD_STATE_OPEN,
    PSD_STATE_CLOSING,
    PSD_STATE_CLOSED,
    PSD_STATE_FAULT,
    PSD_STATE_ESTOP,
    PSD_STATE_COUNT     /* 실제 상태 아님. 범위검사용 개수. */
} psd_state_t;

/* ------------------------------------------------------------------ */
/* TRAIN -> PSD 명령 (TRAIN이 PSD 도어에 내리는 지시)                    */
/* ------------------------------------------------------------------ */

typedef enum
{
    PSD_CMD_NONE = 0,
    PSD_CMD_OPEN,
    PSD_CMD_CLOSE,
    PSD_COMMAND_COUNT    /* 실제 명령 아님. 범위검사용 개수. */
} psd_command_t;

/* ------------------------------------------------------------------ */
/* PSD -> TRAIN 요청 (PSD가 TRAIN에게 보내는 요청)                       */
/* ------------------------------------------------------------------ */

typedef enum
{
    PSD_REQUEST_NONE = 0,
    PSD_REQUEST_START,
    PSD_REQUEST_RECOVERY,
    PSD_REQUEST_COUNT    /* 실제 요청 아님. 범위검사용 개수. */
} psd_request_t;

/* ------------------------------------------------------------------ */
/* 공통 오류 코드                                                       */
/* ------------------------------------------------------------------ */

typedef enum
{
    FAULT_CODE_NONE = 0,
    FAULT_CODE_ESTOP,
    FAULT_CODE_COMM,
    FAULT_CODE_PEER_REBOOT,
    FAULT_CODE_HALL,
    FAULT_CODE_ALIGN,
    FAULT_CODE_TRAIN_DOOR,
    FAULT_CODE_PSD_DOOR,
    FAULT_CODE_COUNT      /* 실제 오류코드 아님. 범위검사용 개수. */
} fault_code_t;

/* ------------------------------------------------------------------ */
/* PSD -> TRAIN 패킷의 flags 비트필드                                   */
/* ------------------------------------------------------------------ */

#define PSD_FLAG_NONE               UINT8_C(0x00)
#define PSD_FLAG_CLOSED_CONFIRMED   UINT8_C(0x01)
#define PSD_FLAG_ESTOP_ACTIVE       UINT8_C(0x02)
/* Display only: 0=debug (boot default), 1=normal. Never an FSM permission. */
#define PSD_FLAG_UI_NORMAL          UINT8_C(0x04)

/* packet.c가 "예약된 flag 비트 검사"에 사용하는 마스크.
 * 여기 없는 상위 비트가 하나라도 켜져 있으면 PACKET_CHECK_BAD_FLAGS. */
#define PSD_FLAGS_VALID_MASK        \
    ((uint8_t)(PSD_FLAG_CLOSED_CONFIRMED | PSD_FLAG_ESTOP_ACTIVE | PSD_FLAG_UI_NORMAL))

/* ------------------------------------------------------------------ */
/* COUNTDOWN 필드 규칙                                                  */
/* ------------------------------------------------------------------ */

/* COUNTDOWN 상태에서 허용하는 최대값 (0~5초 카운트다운).
 * train_config.h의 TRAIN_COUNTDOWN_START_SECONDS 기본값(5)과 일치해야 한다. */
#define COUNTDOWN_MAX_SECONDS       UINT8_C(5)

/* COUNTDOWN 상태가 아닐 때 countdown 필드가 반드시 가져야 하는 값.
 * 0~5 범위 밖의 값으로 골라서 "카운트다운 중이 아님"을 명확히 구분한다. */
#define COUNTDOWN_INACTIVE          UINT8_C(0xFF)

/* ------------------------------------------------------------------ */
/* 무선으로 나가는 실제 패킷 레이아웃 (각 6바이트 고정)                    */
/* ------------------------------------------------------------------ */

typedef struct
{
    uint8_t magic;
    uint8_t version;
    uint8_t train_state;   /* train_state_t */
    uint8_t psd_command;   /* psd_command_t */
    uint8_t countdown;
    uint8_t fault_code;    /* fault_code_t */
} __attribute__((packed)) train_to_psd_packet_t;

typedef struct
{
    uint8_t magic;
    uint8_t version;
    uint8_t psd_state;     /* psd_state_t */
    uint8_t request;       /* psd_request_t */
    uint8_t flags;         /* PSD_FLAG_* 비트OR */
    uint8_t fault_code;    /* fault_code_t */
} __attribute__((packed)) psd_to_train_packet_t;

#endif /* PROTOCOL_H */
