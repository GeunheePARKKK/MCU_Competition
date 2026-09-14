#include <Arduino.h>
#include <string.h>
#include "train_fsm.h"
#include "psd_fsm.h"
#include "train_config.h"
#include "psd_config.h"
#include "hall_detector.h"
#include "packet.h"
#include "comm.h"
#include "motor.h"
#include "debounce.h"
#include "door_toggle.h"
static uint16_t checked, failed;
#define CHECK(c) do { ++checked; if (!(c)) { ++failed; Serial.print(F("FAIL line ")); Serial.println(__LINE__); } } while(0)
static train_fsm_t t;
static psd_fsm_t p;
static train_fsm_inputs_t ti;
static psd_fsm_inputs_t pi;
static train_fsm_outputs_t to;
static psd_fsm_outputs_t po;
static uint32_t clockMs;
extern void fakeRx(const psd_to_train_packet_t &packet);
static void validPackets() {
    train_to_psd_packet_t a;
    psd_to_train_packet_t b;
    packet_build_train_to_psd(&a,to.state,to.psd_command,to.countdown,to.fault_code);
    packet_build_psd_to_train(&b,po.state,po.request,po.flags,po.fault_code);
    CHECK(packet_train_to_psd_is_valid(&a));
    CHECK(packet_psd_to_train_is_valid(&b));
}
static void tick(uint32_t elapsed = 10) {
    clockMs += elapsed;
    ti.psd_state=po.state; ti.psd_request=po.request; ti.psd_fault_code=po.fault_code;
    ti.psd_closed_confirmed=!!(po.flags & PSD_FLAG_CLOSED_CONFIRMED);
    ti.psd_estop_active=!!(po.flags & PSD_FLAG_ESTOP_ACTIVE);
    train_fsm_update(&t,&ti,clockMs,&to);
    pi.train_state=to.state; pi.train_command=to.psd_command; pi.train_fault_code=to.fault_code;
    psd_fsm_update(&p,&pi,clockMs,&po);
    validPackets();
    pi.start_pressed=0; ti.m2_detected=0; ti.m4_detected=0;
}
static void resetPair() {
    memset(&ti,0,sizeof(ti)); memset(&pi,0,sizeof(pi));
    memset(&to,0,sizeof(to)); memset(&po,0,sizeof(po));
    clockMs=0;
    train_fsm_init(&t,0,0); psd_fsm_init(&p,0,0);
    to.countdown=COUNTDOWN_INACTIVE;
    ti.init_complete=ti.communication_ok=ti.peer_state_valid=1;
    pi.init_complete=pi.communication_ok=pi.peer_state_valid=1;
    ti.hall_ok=ti.at_m1=ti.train_door_closed=1;
    pi.door_closed=1; po.flags=PSD_FLAG_CLOSED_CONFIRMED;
    tick(); tick(); tick();
    CHECK(to.state==TRAIN_STATE_READY && po.state==PSD_STATE_READY);
}
static void startPair() {
    pi.start_pressed=1; tick(); tick();
    ti.at_m1=0;
    CHECK(to.state==TRAIN_STATE_RUNNING && to.motor_command==MOTOR_COMMAND_RUN);
}
static void fullCycle() {
    resetPair(); startPair();
    ti.m2_detected=1; tick();
    CHECK(to.state==TRAIN_STATE_APPROACH && to.motor_command==MOTOR_COMMAND_APPROACH);
    ti.m4_detected=1; ti.aligned=1; tick(); tick(301);
    CHECK(to.state==TRAIN_STATE_DOOR_OPENING && to.motor_command==MOTOR_COMMAND_STOP);
    CHECK(to.psd_command==PSD_CMD_NONE && po.door_command==PSD_DOOR_CLOSE);
    tick(1500); CHECK(to.state==TRAIN_STATE_DOOR_OPENING);
    ti.train_door_closed=0; tick();
    CHECK(to.state==TRAIN_STATE_WAIT_PSD_OPEN && po.state==PSD_STATE_OPENING);
    CHECK(to.door_command==TRAIN_DOOR_OPEN);
    pi.door_closed=0; tick(1001); tick();
    CHECK(to.state==TRAIN_STATE_DWELL && po.state==PSD_STATE_OPEN);
    tick(5000); CHECK(to.countdown==5);
    for (uint8_t c=4; c<5; --c) { tick(1000); CHECK(to.countdown==c); }
    tick(301); CHECK(to.state==TRAIN_STATE_DOOR_CLOSING);
    CHECK(po.state==PSD_STATE_OPEN);
    tick(); ti.train_door_closed=1; tick();
    CHECK(to.state==TRAIN_STATE_WAIT_PSD_CLOSED && po.state==PSD_STATE_CLOSING);
    tick(); pi.door_closed=1; tick(); tick();
    CHECK(to.state==TRAIN_STATE_COMPLETE && po.state==PSD_STATE_CLOSED);
    tick(1000); CHECK(to.state==TRAIN_STATE_COMPLETE && to.motor_command==MOTOR_COMMAND_STOP);
    ti.at_m1=1; tick(); tick();
    CHECK(to.state==TRAIN_STATE_READY && to.motor_command==MOTOR_COMMAND_STOP);
    startPair();
}
static void faultsAndRecovery() {
    resetPair(); startPair();
    // E-STOP during a still-held START request must remain a valid packet.
    pi.estop_active=1; tick(); tick();
    CHECK(to.state==TRAIN_STATE_ESTOP && po.state==PSD_STATE_ESTOP);
    CHECK(po.request==PSD_REQUEST_NONE && to.motor_command==MOTOR_COMMAND_STOP);
    pi.estop_active=0; tick(1000);
    CHECK(to.state==TRAIN_STATE_ESTOP);
    ti.at_m1=1; pi.start_pressed=1; tick(); tick(); tick(501); tick();
    CHECK(to.state==TRAIN_STATE_READY && po.state==PSD_STATE_READY);
    CHECK(to.motor_command==MOTOR_COMMAND_STOP && po.request==PSD_REQUEST_NONE);

    resetPair(); startPair(); tick(TRAIN_RUNNING_TIMEOUT_MS);
    CHECK(to.state==TRAIN_STATE_FAULT && to.fault_code==FAULT_CODE_HALL);
    resetPair(); startPair(); ti.m2_detected=1; tick(); tick(TRAIN_APPROACH_TIMEOUT_MS);
    CHECK(to.fault_code==FAULT_CODE_HALL);
    resetPair(); startPair(); ti.train_door_closed=0; tick();
    CHECK(to.fault_code==FAULT_CODE_TRAIN_DOOR && to.motor_command==MOTOR_COMMAND_STOP);
    resetPair(); startPair(); pi.door_closed=0; tick(); tick();
    CHECK(to.fault_code==FAULT_CODE_PSD_DOOR);
    resetPair(); startPair(); ti.communication_timed_out=1; tick();
    CHECK(to.fault_code==FAULT_CODE_COMM && to.motor_command==MOTOR_COMMAND_STOP);

    resetPair(); t.state=TRAIN_STATE_DOOR_OPENING; t.state_enter_ms=clockMs;
    tick(TRAIN_DOOR_OPEN_TIMEOUT_MS);
    CHECK(to.fault_code==FAULT_CODE_TRAIN_DOOR); // held KW cannot fake opening
    resetPair(); t.state=TRAIN_STATE_COASTING; t.state_enter_ms=clockMs; ti.aligned=0;
    tick(301); CHECK(to.fault_code==FAULT_CODE_ALIGN);

    // A stale already-pressed KW cannot complete a new closing command.
    resetPair(); t.state=TRAIN_STATE_DOOR_CLOSING; t.close_armed=0; t.state_enter_ms=clockMs;
    p.state=PSD_STATE_OPEN; po.state=PSD_STATE_OPEN;
    tick(); CHECK(to.state==TRAIN_STATE_DOOR_CLOSING);
    ti.train_door_closed=0; tick(); ti.train_door_closed=1; tick();
    CHECK(to.state==TRAIN_STATE_WAIT_PSD_CLOSED);
    // CLOSED flag alone is insufficient while peer is still CLOSING.
    po.state=PSD_STATE_CLOSING; po.flags=PSD_FLAG_CLOSED_CONFIRMED;
    tick(); CHECK(to.state==TRAIN_STATE_WAIT_PSD_CLOSED);

    // Repeated recovery packets stay valid and do not get dropped on INIT->READY.
    resetPair(); p.state=PSD_STATE_FAULT; p.fault_code=FAULT_CODE_COMM;
    pi.start_pressed=1; tick(); CHECK(po.request==PSD_REQUEST_RECOVERY);
    tick(100); CHECK(po.request==PSD_REQUEST_RECOVERY);
    tick(501); CHECK(po.state==PSD_STATE_READY && po.request==PSD_REQUEST_NONE);
}
static void hallTests() {
    hall_detector_t h;
    const uint16_t values[]={529,489,555,462,587,434,443,573};
    const uint8_t ids[]={1,1,2,2,3,3,3,3};
    for (uint8_t i=0;i<sizeof(ids)/sizeof(ids[0]);++i) {
        hall_detector_init(&h,0);
        hall_detector_update(&h,values[i],ids[i],0);
        hall_detector_update(&h,values[i],ids[i],29);
        CHECK(h.event==0);
        hall_detector_update(&h,values[i],ids[i],30);
        CHECK(h.event==ids[i]);
        hall_detector_update(&h,values[i],ids[i],100);
        CHECK(h.event==0);
    }
    CHECK(hall_classify(15)==1 && hall_classify(25)==1 && hall_classify(26)==255);
    CHECK(hall_classify(41)==2 && hall_classify(51)==2 && hall_classify(40)==255);
    CHECK(hall_classify(72)==3 && hall_classify(82)==3 && hall_classify(83)==255);
    CHECK(hall_classify(59)==255 && hall_classify(60)==3);
    CHECK(hall_classify(65)==3 && hall_classify(70)==3 && hall_classify(71)==3);
    for (uint16_t gap=52;gap<60;++gap) CHECK(hall_classify(gap)==255);
    hall_detector_init(&h,0);
    hall_detector_update(&h,529,1,0); hall_detector_update(&h,529,1,30);
    hall_detector_update(&h,555,2,40); hall_detector_update(&h,555,2,100);
    CHECK(h.event==0 && !h.armed); // no zero gap, cannot count the same group twice
    hall_detector_update(&h,508,2,110); hall_detector_update(&h,508,2,259);
    CHECK(!h.armed);
    hall_detector_update(&h,508,2,260); CHECK(h.armed);
    hall_detector_update(&h,555,2,270); hall_detector_update(&h,555,2,300);
    CHECK(h.event==2);
    hall_detector_update(&h,508,3,310); hall_detector_update(&h,508,3,460);
    hall_detector_update(&h,529,3,470); hall_detector_update(&h,529,3,510);
    CHECK(h.event==0);
    hall_detector_update(&h,555,3,520); hall_detector_update(&h,555,3,560);
    CHECK(h.event==0);
    hall_detector_update(&h,587,3,570); hall_detector_update(&h,587,3,610);
    CHECK(h.event==3);
    hall_detector_update(&h,1023,0,620); hall_detector_update(&h,1023,0,870);
    CHECK(h.rail_fault); hall_detector_update(&h,508,0,880); CHECK(!h.rail_fault);
    hall_detector_init(&h,0xfffffff0UL);
    hall_detector_update(&h,529,1,0xfffffff0UL);
    hall_detector_update(&h,529,1,0x20UL);
    CHECK(h.event==1); // unsigned timer wrap
}
static void commTests() {
    comm_t c;
    train_fsm_inputs_t in={};
    train_fsm_outputs_t out={};
    out.state=TRAIN_STATE_INIT; out.countdown=COUNTDOWN_INACTIVE;
    comm_init(&c,COMM_ROLE_TRAIN,0);
    comm_train_update(&c,&out,60000,&in);
    CHECK(!in.communication_ok && !in.communication_timed_out);
    psd_to_train_packet_t packet;
    packet_build_psd_to_train(&packet,PSD_STATE_READY,PSD_REQUEST_NONE,
                             PSD_FLAG_CLOSED_CONFIRMED,FAULT_CODE_NONE);
    fakeRx(packet);
    comm_train_update(&c,&out,60010,&in);
    CHECK(in.communication_ok && in.peer_state_valid);
    comm_train_update(&c,&out,62009,&in); CHECK(in.communication_ok);
    comm_train_update(&c,&out,62010,&in);
    CHECK(in.communication_timed_out && !in.communication_ok && !in.peer_state_valid);
    packet.version=1; fakeRx(packet);
    comm_train_update(&c,&out,63000,&in);
    CHECK(c.invalid_rx_count==1 && in.communication_timed_out);
    packet.version=2; fakeRx(packet);
    comm_train_update(&c,&out,63010,&in);
    CHECK(c.invalid_rx_count==2 && in.communication_timed_out);
    packet_build_psd_to_train(&packet,PSD_STATE_INIT,PSD_REQUEST_NONE,0,FAULT_CODE_NONE);
    fakeRx(packet); out.state=TRAIN_STATE_RUNNING;
    comm_train_update(&c,&out,64000,&in);
    CHECK(in.peer_reboot_detected);
}
static void hardwareHelpers() {
    motor_config_t cfg={200,100};
    motor_init();
    CHECK(!(TCCR2A & (1 << COM2B1)) && !(PORTD & (1 << PD3)));
    motor_apply_command(MOTOR_CMD_RUN,&cfg);
    CHECK(OCR2B==200 && (TCCR2A & (1 << COM2B1)));
    motor_apply_command(MOTOR_CMD_APPROACH,&cfg); CHECK(OCR2B==100);
    motor_apply_command(MOTOR_CMD_STOP,&cfg);
    CHECK(OCR2B==0 && !(TCCR2A & (1 << COM2B1)) && !(PORTD & (1 << PD3)));
    debounce_t button;
    debounce_init(&button,0,DEBOUNCE_LEVEL_LOW,30,0);
    CHECK(!debounce_take_pressed(&button)); // held START at boot cannot start
    debounce_update(&button,1,10); debounce_update(&button,1,40);
    debounce_update(&button,0,50); debounce_update(&button,0,79);
    CHECK(!debounce_take_pressed(&button));
    debounce_update(&button,0,80); CHECK(debounce_take_pressed(&button));
    CHECK(!debounce_take_pressed(&button));
    debounce_init(&button,0,DEBOUNCE_LEVEL_HIGH,40,0); // NC KW11
    debounce_update(&button,1,10); debounce_update(&button,1,50);
    CHECK(debounce_is_active(&button));
}
static void clickDoor(door_toggle_t *d, uint32_t now) {
    door_toggle_update(d,1,now);
    door_toggle_update(d,1,now+40);
    door_toggle_update(d,0,now+60);
    door_toggle_update(d,0,now+100);
}
static void toggleTests() {
    door_toggle_t a,b;
    door_toggle_init(&a,0,40,0);
    door_toggle_init(&b,0,40,0);
    CHECK(!a.closed && !b.closed);
    door_toggle_update(&a,1,10); door_toggle_update(&a,1,50);
    CHECK(!a.closed); // latch changes on release, not on press
    door_toggle_update(&a,1,5000); CHECK(!a.closed);
    door_toggle_update(&a,0,5010); door_toggle_update(&a,0,5050);
    CHECK(a.closed && !b.closed);
    door_toggle_update(&a,0,9000); CHECK(a.closed);
    clickDoor(&b,10000); CHECK(a.closed && b.closed);
    clickDoor(&a,11000); CHECK(!a.closed && b.closed);
    // Bounce / too-short press must not toggle.
    door_toggle_update(&a,1,12000);
    door_toggle_update(&a,0,12020);
    door_toggle_update(&a,0,12100); CHECK(!a.closed);
    // Holding the physical switch at boot is not a synthetic press.
    door_toggle_init(&a,1,40,0);
    door_toggle_update(&a,0,10); door_toggle_update(&a,0,50);
    CHECK(!a.closed); clickDoor(&a,100); CHECK(a.closed);
    door_toggle_init(&a,0,40,0xffffff00UL);
    clickDoor(&a,0xfffffff0UL); CHECK(a.closed);

    // Latched inputs permit two doors to be restored sequentially in ESTOP.
    resetPair(); startPair();
    pi.estop_active=1; pi.start_pressed=1; tick(); tick();
    CHECK(po.state==PSD_STATE_ESTOP && po.request==PSD_REQUEST_NONE);
    pi.estop_active=0;
    door_toggle_init(&a,0,40,0); door_toggle_init(&b,0,40,0);
    ti.train_door_closed=a.closed; pi.door_closed=b.closed;
    pi.start_pressed=1; tick(); CHECK(po.state==PSD_STATE_ESTOP);
    clickDoor(&a,100); ti.train_door_closed=a.closed;
    tick(); CHECK(to.state==TRAIN_STATE_ESTOP && a.closed);
    clickDoor(&b,300); pi.door_closed=b.closed;
    ti.at_m1=1; tick();
    CHECK(to.state==TRAIN_STATE_ESTOP && po.state==PSD_STATE_ESTOP);
    pi.start_pressed=1; tick(); tick(); tick(501); tick();
    CHECK(to.state==TRAIN_STATE_READY && po.state==PSD_STATE_READY);
    CHECK(to.motor_command==MOTOR_COMMAND_STOP && a.closed && b.closed);
}
static void stop443Regression() {
    hall_detector_t h;
    resetPair(); startPair();
    ti.m2_detected=1; tick();
    hall_detector_init(&h,clockMs);
    hall_detector_update(&h,443,3,clockMs);
    hall_detector_update(&h,443,3,clockMs+30);
    ti.m4_detected=h.event==3;
    ti.aligned=h.stable_marker==3;
    tick(30);
    CHECK(to.state==TRAIN_STATE_COASTING);
    hall_detector_update(&h,443,0,clockMs+301);
    ti.aligned=h.stable_marker==3;
    tick(301);
    CHECK(to.state==TRAIN_STATE_DOOR_OPENING && to.fault_code==FAULT_CODE_NONE);
}

static void closedRecheckTests() {
    // The audit's original bug: TRAIN reopens while PSD is still closing.
    resetPair();
    t.state=TRAIN_STATE_WAIT_PSD_CLOSED; t.state_enter_ms=clockMs;
    p.state=PSD_STATE_CLOSING; p.state_enter_ms=clockMs;
    po.state=PSD_STATE_CLOSING; pi.door_closed=0; po.flags=0;
    tick(); CHECK(to.state==TRAIN_STATE_WAIT_PSD_CLOSED);
    ti.train_door_closed=0; tick();
    CHECK(to.state==TRAIN_STATE_FAULT && to.fault_code==FAULT_CODE_TRAIN_DOOR);
    CHECK(to.motor_command==MOTOR_COMMAND_STOP);

    // Reopening on the very frame where PSD reports CLOSED must not produce DONE.
    resetPair();
    t.state=TRAIN_STATE_WAIT_PSD_CLOSED;
    p.state=PSD_STATE_CLOSED; po.state=PSD_STATE_CLOSED;
    ti.train_door_closed=0; tick();
    CHECK(to.state==TRAIN_STATE_FAULT && to.fault_code==FAULT_CODE_TRAIN_DOOR);

    // PSD's CLOSED self-check works even before its next transmitted packet.
    resetPair(); p.state=PSD_STATE_CLOSED; pi.door_closed=0; tick();
    CHECK(po.state==PSD_STATE_FAULT && po.fault_code==FAULT_CODE_PSD_DOOR);
    CHECK(!(po.flags & PSD_FLAG_CLOSED_CONFIRMED));

    resetPair(); t.state=TRAIN_STATE_COMPLETE; ti.at_m1=0;
    p.state=PSD_STATE_CLOSED; po.state=PSD_STATE_CLOSED;
    ti.train_door_closed=0; tick();
    CHECK(to.fault_code==FAULT_CODE_TRAIN_DOOR);

    resetPair(); t.state=TRAIN_STATE_COMPLETE; ti.at_m1=0;
    p.state=PSD_STATE_CLOSED; po.state=PSD_STATE_CLOSED;
    pi.door_closed=0; tick(); tick();
    CHECK(to.state==TRAIN_STATE_FAULT && to.fault_code==FAULT_CODE_PSD_DOOR);
    CHECK(to.motor_command==MOTOR_COMMAND_STOP);
}

extern void fakeRxTrain(const train_to_psd_packet_t &packet);
extern void fakeLastTx(uint8_t *out);
static void uiProtocolTests() {
    CHECK(PROTOCOL_VERSION==4 && sizeof(psd_to_train_packet_t)==6);
    psd_to_train_packet_t packet;
    for (uint8_t mode=0;mode<2;++mode) {
        uint8_t flags=PSD_FLAG_CLOSED_CONFIRMED | (mode ? PSD_FLAG_UI_NORMAL : 0);
        packet_build_psd_to_train(&packet,PSD_STATE_READY,PSD_REQUEST_NONE,flags,FAULT_CODE_NONE);
        CHECK(packet_psd_to_train_is_valid(&packet));
        packet.version=3; CHECK(packet_validate_psd_to_train(&packet)==PACKET_CHECK_BAD_VERSION);
        packet.version=4; packet.flags|=0x08;
        CHECK(packet_validate_psd_to_train(&packet)==PACKET_CHECK_BAD_FLAGS);
        packet_build_psd_to_train(&packet,PSD_STATE_ESTOP,PSD_REQUEST_NONE,
            flags|PSD_FLAG_ESTOP_ACTIVE,FAULT_CODE_ESTOP);
        CHECK(packet_psd_to_train_is_valid(&packet));
        packet.flags &= ~PSD_FLAG_ESTOP_ACTIVE;
        CHECK(!packet_psd_to_train_is_valid(&packet));
    }
    comm_t c; train_fsm_outputs_t out={}; train_fsm_inputs_t in={};
    out.state=TRAIN_STATE_INIT; out.countdown=COUNTDOWN_INACTIVE;
    comm_init(&c,COMM_ROLE_TRAIN,0); CHECK(!c.ui_normal);
    packet_build_psd_to_train(&packet,PSD_STATE_READY,PSD_REQUEST_NONE,
        PSD_FLAG_CLOSED_CONFIRMED|PSD_FLAG_UI_NORMAL,FAULT_CODE_NONE);
    fakeRx(packet); comm_train_update(&c,&out,10,&in);
    CHECK(c.ui_normal && in.psd_closed_confirmed && !in.psd_estop_active);
    packet.flags=0; packet.version=3;
    fakeRx(packet); comm_train_update(&c,&out,20,&in);
    CHECK(c.ui_normal && in.psd_closed_confirmed); // Old packet cannot change either.
    comm_train_update(&c,&out,2010,&in);
    CHECK(!in.communication_ok && c.ui_normal); // Keep last mode, never hide link fault.
    packet.version=4; packet.flags=PSD_FLAG_CLOSED_CONFIRMED;
    fakeRx(packet); comm_train_update(&c,&out,2020,&in);
    CHECK(in.communication_ok && !c.ui_normal && in.psd_closed_confirmed);
    // Mode bit alone must not authorize closed doors, START, or recovery.
    packet.flags=PSD_FLAG_UI_NORMAL;
    fakeRx(packet); comm_train_update(&c,&out,2030,&in);
    CHECK(c.ui_normal && !in.psd_closed_confirmed && in.psd_request==PSD_REQUEST_NONE);

    comm_init(&c,COMM_ROLE_PSD,0); c.ui_normal=1;
    psd_fsm_outputs_t pOut={}; pOut.state=PSD_STATE_READY;
    pOut.flags=PSD_FLAG_CLOSED_CONFIRMED;
    comm_psd_transmit(&c,&pOut,25); fakeLastTx((uint8_t*)&packet);
    CHECK(packet_psd_to_train_is_valid(&packet));
    CHECK(packet.flags==(PSD_FLAG_CLOSED_CONFIRMED|PSD_FLAG_UI_NORMAL));
    CHECK(pOut.flags==PSD_FLAG_CLOSED_CONFIRMED && pOut.request==PSD_REQUEST_NONE);
    comm_psd_transmit(&c,&pOut,75); fakeLastTx((uint8_t*)&packet);
    CHECK(packet.flags & PSD_FLAG_UI_NORMAL); // Mode is repeated, not a lossy toggle event.
    c.ui_normal=0;
    comm_psd_transmit(&c,&pOut,125); fakeLastTx((uint8_t*)&packet);
    CHECK(packet.flags==PSD_FLAG_CLOSED_CONFIRMED);
    pOut.state=PSD_STATE_ESTOP; pOut.fault_code=FAULT_CODE_ESTOP;
    pOut.flags|=PSD_FLAG_ESTOP_ACTIVE; c.ui_normal=1;
    comm_psd_transmit(&c,&pOut,175); fakeLastTx((uint8_t*)&packet);
    CHECK(packet_psd_to_train_is_valid(&packet));
    CHECK(packet.flags==(PSD_FLAG_CLOSED_CONFIRMED|PSD_FLAG_ESTOP_ACTIVE|PSD_FLAG_UI_NORMAL));
}

void setup() {
    Serial.begin(115200);
    fullCycle(); faultsAndRecovery(); hallTests(); commTests(); hardwareHelpers(); toggleTests();
    stop443Regression(); closedRecheckTests(); uiProtocolTests();
    Serial.print(F("CHECKS ")); Serial.println(checked);
    Serial.print(F("FAILED ")); Serial.println(failed);
    Serial.println(failed ? F("TESTS FAIL") : F("TESTS PASS"));
}
void loop() {}
