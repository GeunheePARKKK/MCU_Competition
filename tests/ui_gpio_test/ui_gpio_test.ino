// Emulator only: exercise the production indicators AND physical output driver.
#include <Arduino.h>
#include "psd_outputs.h"
static PsdOutputs pins;
static PsdIndicators indicators;
static UiSnapshot s;
static void emitFrame() {
    const PsdIndicatorFrame f=indicators.update(s,millis());
    pins.apply(f);
    Serial.print(F("FRAME ")); Serial.print(f.word,HEX);
    Serial.print(' '); Serial.println(f.hz);
    Serial.flush();
    delay(80); // Test harness only; production has no sequence delays.
}
void setup() {
    Serial.begin(115200);
    pins.begin();
    Serial.println(F("BOOT")); Serial.flush();
    s.link=s.peerValid=true; s.closed=1;
    s.psdState=PSD_STATE_READY; s.countdown=COUNTDOWN_INACTIVE;
    const train_state_t phases[]={TRAIN_STATE_READY,TRAIN_STATE_RUNNING,
        TRAIN_STATE_APPROACH,TRAIN_STATE_COASTING,TRAIN_STATE_DOOR_OPENING};
    for(uint8_t i=0;i<5;++i) { s.trainState=phases[i]; emitFrame(); }
    s.trainState=TRAIN_STATE_WAIT_PSD_OPEN; s.psdState=PSD_STATE_OPENING; emitFrame();
    s.trainState=TRAIN_STATE_DWELL; s.psdState=PSD_STATE_OPEN; emitFrame();
    s.trainState=TRAIN_STATE_COUNTDOWN;
    for(uint8_t c=5;c<=5;--c) { s.countdown=c; emitFrame(); }
    s.countdown=COUNTDOWN_INACTIVE; s.trainState=TRAIN_STATE_DOOR_CLOSING; emitFrame();
    s.trainState=TRAIN_STATE_WAIT_PSD_CLOSED; s.psdState=PSD_STATE_CLOSING; emitFrame();
    s.trainState=TRAIN_STATE_COMPLETE; s.psdState=PSD_STATE_CLOSED; emitFrame();
    s.psdState=PSD_STATE_FAULT; s.fault=FAULT_CODE_PSD_DOOR; emitFrame();
    s.psdState=PSD_STATE_ESTOP; s.fault=FAULT_CODE_ESTOP; emitFrame();
    s.psdState=PSD_STATE_INIT; s.fault=0; s.link=s.peerValid=false; emitFrame();
    Serial.println(F("GPIO DONE")); Serial.flush();
}
void loop() {}
