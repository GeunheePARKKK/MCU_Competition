#include <string.h>
#include "nrf24.h"
#include "protocol.h"
static bool pending;
static uint8_t data[6], lastSent[6];
void fakeRxTrain(const train_to_psd_packet_t &packet) { memcpy(data,&packet,6); pending=true; }
void fakeLastTx(uint8_t *out) { memcpy(out,lastSent,6); }
void fakeRx(const psd_to_train_packet_t &packet) { memcpy(data,&packet,6); pending=true; }
nrf24_result_t nrf24_init(nrf24_role_t) { pending=false; return NRF24_RESULT_OK; }
nrf24_result_t nrf24_update_tx(uint32_t) { return NRF24_RESULT_NO_DATA; }
uint8_t nrf24_is_tx_busy(void) { return 0; }
nrf24_result_t nrf24_start_send(const uint8_t *packet,uint32_t) { memcpy(lastSent,packet,6); return NRF24_RESULT_OK; }
nrf24_result_t nrf24_receive(uint8_t *out) {
    if (!pending) return NRF24_RESULT_NO_DATA;
    pending=false; memcpy(out,data,6); return NRF24_RESULT_OK;
}
uint8_t nrf24_read_status(void) { return 0; }
