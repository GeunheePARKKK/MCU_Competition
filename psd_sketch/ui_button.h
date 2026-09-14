#ifndef TRAIN_PSD_UI_BUTTON_H
#define TRAIN_PSD_UI_BUTTON_H
#include <stdint.h>

// Active-low push button: one event after a stable press and release.
// Holding at boot, switch bounce and holding during operation do not repeat.
class UiModeButton {
public:
    void begin(bool pressed, uint32_t now) {
        raw_ = stable_ = pressed;
        ready_ = !pressed; armed_ = false; changed_ = now;
    }
    bool update(bool pressed, uint32_t now) {
        if (pressed != raw_) { raw_ = pressed; changed_ = now; }
        if (raw_ == stable_ || uint32_t(now - changed_) < 30) return false;
        stable_ = raw_;
        if (stable_) { armed_ = ready_; return false; }
        const bool event = armed_ && ready_;
        ready_ = true; armed_ = false;
        return event;
    }
private:
    bool raw_ = false, stable_ = false, ready_ = true, armed_ = false;
    uint32_t changed_ = 0;
};
#endif
