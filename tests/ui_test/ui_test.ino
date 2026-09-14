// Emulator only. Include the real PSD headers via compiler.cpp.extra_flags -I.
#include <Arduino.h>
#include <string.h>
#include "ui_display.h"
#include "ui_button.h"
#include "psd_indicators.h"
static uint16_t checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; Serial.print(F("FAIL line ")); Serial.println(__LINE__); } } while (0)
static UiSnapshot snapshot() {
    UiSnapshot s={}; s.link=s.peerValid=true; s.closed=s.peerClosed=1;
    s.trainState=TRAIN_STATE_READY; s.psdState=PSD_STATE_READY;
    s.countdown=COUNTDOWN_INACTIVE; s.adc=443; s.delta=65; s.marker=3;
    return s;
}
static uint16_t lit(const PsdIndicatorFrame &f) { return f.word&0x01FF; }
static uint8_t bars(const PsdIndicatorFrame &f) { return f.word>>9; }
static void displayTests() {
    char top[17], bottom[17];
    for (uint8_t local=0;local<2;++local) for(uint8_t mode=0;mode<2;++mode) {
        UiSnapshot s=snapshot(); s.trainLocal=local; s.normal=mode;
        for(uint8_t st=0;st<TRAIN_STATE_COUNT;++st) {
            s.trainState=train_state_t(st);
            s.fault=st==TRAIN_STATE_FAULT ? FAULT_CODE_TRAIN_DOOR :
                    st==TRAIN_STATE_ESTOP ? FAULT_CODE_ESTOP : FAULT_CODE_NONE;
            s.peerFault=s.fault;
            s.countdown=st==TRAIN_STATE_COUNTDOWN ? 5 : COUNTDOWN_INACTIVE;
            for(uint8_t page=0;page<2;++page) {
                uiFormatLcd(s,2000UL*page,top,bottom);
                CHECK(strlen(top)>0 && strlen(top)<=16);
                CHECK(strlen(bottom)>0 && strlen(bottom)<=16);
                if(!mode && local && !s.fault && st!=TRAIN_STATE_COUNTDOWN)
                    CHECK(strstr(top,"E0")!=0); // Long T-CLOSE labels must fit.
                if(st==TRAIN_STATE_FAULT) CHECK(strstr(top,"F6")!=0);
                if(st==TRAIN_STATE_ESTOP) CHECK(strstr(top,"F1")!=0);
            }
        }
        s=snapshot(); s.normal=mode; s.trainLocal=local;
        s.trainState=TRAIN_STATE_COUNTDOWN; s.psdState=PSD_STATE_OPEN;
        for(uint8_t c=0;c<=5;++c) {
            s.countdown=c;
            for(uint8_t page=0;page<2;++page) {
                uiFormatLcd(s,2000UL*page,top,bottom);
                CHECK(uiCount(s)==c);
                CHECK(strchr(top,'0'+c)!=0);
                CHECK(strstr(top,mode ? "CLOSE IN" : "COUNT")!=0);
            }
        }
        s.link=false; uiFormatLcd(s,2000,top,bottom);
        CHECK(uiCount(s)==COUNTDOWN_INACTIVE);
        if(mode) CHECK(strstr(top,"WAIT LINK")!=0);
    }
    UiSnapshot s=snapshot(); s.trainLocal=true; s.normal=false;
    s.trainState=TRAIN_STATE_FAULT; s.fault=FAULT_CODE_ALIGN;
    uiFormatLcd(s,0,top,bottom); CHECK(strstr(bottom,"A443")!=0);
    CHECK(strstr(top,"F5")!=0);
    uiFormatLcd(s,2000,top,bottom); CHECK(strstr(top,"F5")!=0 && strstr(bottom,"V1 P1")!=0);
    s=snapshot(); s.psdState=PSD_STATE_ESTOP; s.fault=FAULT_CODE_ESTOP; s.normal=true;
    uiFormatLcd(s,0,top,bottom); CHECK(strstr(bottom,"E0")!=0);
    s.estopPressed=1; uiFormatLcd(s,0,top,bottom); CHECK(strstr(bottom,"E1")!=0);
}
static void indicatorTests() {
    CHECK(PSD_INDICATORS_OFF==0x0000);
    CHECK(PSD_LED_MASK==0x01FF);
    UiSnapshot s=snapshot(); PsdIndicators ind;
    PsdIndicatorFrame f=ind.update(s,0);
    CHECK(lit(f)==3 && bars(f)==0 && f.hz==0); // READY + LINK.
    s.trainState=TRAIN_STATE_RUNNING; f=ind.update(s,1);
    CHECK(lit(f)==6 && bars(f)==0);
    s.trainState=TRAIN_STATE_APPROACH; f=ind.update(s,2); CHECK(lit(f)==10);
    s.trainState=TRAIN_STATE_COASTING; f=ind.update(s,3); CHECK(lit(f)==2);
    s.trainState=TRAIN_STATE_DOOR_OPENING; f=ind.update(s,100);
    CHECK(lit(f)==50 && f.hz==1200);
    f=ind.update(s,200); CHECK(f.hz==1200);
    s.normal=true; f=ind.update(s,300); CHECK(f.hz==0); // Mode does not retrigger sound.
    f=ind.update(s,350); CHECK(f.hz==1600);
    f=ind.update(s,600); CHECK(f.hz==0);
    s.trainState=TRAIN_STATE_WAIT_PSD_OPEN; s.psdState=PSD_STATE_OPENING;
    f=ind.update(s,700); CHECK(lit(f)==82 && bars(f)==0);
    s.trainState=TRAIN_STATE_DWELL; s.psdState=PSD_STATE_OPEN;
    f=ind.update(s,800); CHECK(lit(f)==18);
    const uint8_t masks[6]={0x3F,0x06,0x5B,0x4F,0x66,0x6D};
    s.trainState=TRAIN_STATE_COUNTDOWN;
    for(uint8_t c=5;c<=5;--c) {
        s.countdown=c; uint32_t now=1000UL+(5-c)*1000UL;
        f=ind.update(s,now); CHECK(bars(f)==masks[c] && lit(f)==18);
        CHECK(f.hz==(c ? 1800 : 2200));
        f=ind.update(s,now+250); CHECK(f.hz==0 && bars(f)==masks[c]);
        f=ind.update(s,now+900); CHECK(f.hz==0 && bars(f)==masks[c]);
    }
    s.countdown=COUNTDOWN_INACTIVE; s.trainState=TRAIN_STATE_DOOR_CLOSING;
    f=ind.update(s,7100); CHECK(lit(f)==50 && bars(f)==0);
    s.trainState=TRAIN_STATE_WAIT_PSD_CLOSED; s.psdState=PSD_STATE_CLOSING;
    f=ind.update(s,7200); CHECK(lit(f)==82);
    s.trainState=TRAIN_STATE_COMPLETE; s.psdState=PSD_STATE_CLOSED;
    f=ind.update(s,7300); CHECK(lit(f)==146 && f.hz==1000);
    f=ind.update(s,7500); CHECK(f.hz==1500);
    f=ind.update(s,7600); CHECK(f.hz==2000);
    f=ind.update(s,7800); CHECK(f.hz==0);
    s.fault=FAULT_CODE_TRAIN_DOOR; s.psdState=PSD_STATE_FAULT;
    f=ind.update(s,8000); CHECK(lit(f)==258 && bars(f)==0x79 && f.hz==700);
    f=ind.update(s,8200); CHECK(f.hz==0); // Repeated fault packets do not reset the beep.
    f=ind.update(s,10000); CHECK(f.hz==700);
    s.psdState=PSD_STATE_ESTOP; s.fault=FAULT_CODE_ESTOP;
    f=ind.update(s,10000); CHECK(lit(f)==258 && bars(f)==0 && f.hz==2000);
    f=ind.update(s,10250); CHECK(lit(f)==2 && f.hz==0);
    f=ind.update(s,10300); CHECK(f.hz==2000);
    s.link=false; s.peerValid=false; s.psdState=PSD_STATE_INIT; s.fault=0;
    f=ind.update(s,10400); CHECK(f.word==PSD_INDICATORS_OFF && f.hz==0);
    // No stale countdown or done pattern with a lost peer.
    s.trainState=TRAIN_STATE_COUNTDOWN; s.countdown=3;
    f=ind.update(s,10500); CHECK(bars(f)==0 && lit(f)==0 && !f.hz);
    // Unsigned elapsed-time wrap for a one-shot tone.
    PsdIndicators wrap; s=snapshot(); s.trainState=TRAIN_STATE_DOOR_OPENING;
    f=wrap.update(s,0xFFFFFFF0UL); CHECK(f.hz==1200);
    f=wrap.update(s,0x00000086UL); CHECK(f.hz==0);
}
static void buttonTests() {
    UiModeButton b; b.begin(false,0);
    CHECK(!b.update(true,10)); CHECK(!b.update(false,20)); CHECK(!b.update(false,50));
    CHECK(!b.update(true,100)); CHECK(!b.update(true,130)); CHECK(!b.update(true,10000));
    CHECK(!b.update(false,10010)); CHECK(b.update(false,10040)); CHECK(!b.update(false,11000));
    b.begin(true,0); CHECK(!b.update(false,10)); CHECK(!b.update(false,40));
    CHECK(!b.update(true,50)); CHECK(!b.update(true,80)); CHECK(!b.update(false,90));
    CHECK(b.update(false,120));
    b.begin(false,0xFFFFFF00UL);
    CHECK(!b.update(true,0xFFFFFFF0UL)); CHECK(!b.update(true,0x0E));
    CHECK(!b.update(false,0x20)); CHECK(b.update(false,0x3E));
}
void setup() {
    Serial.begin(115200);
    displayTests(); indicatorTests(); buttonTests();
    Serial.print(F("CHECKS ")); Serial.println(checks);
    Serial.print(F("FAILED ")); Serial.println(failures);
    Serial.println(failures ? F("TESTS FAIL") : F("TESTS PASS"));
    Serial.flush();
}
void loop() {}
