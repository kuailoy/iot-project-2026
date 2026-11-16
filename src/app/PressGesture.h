#pragma once

#include <stdint.h>

// Tells a tap from a hold on a button (the encoder's push button, the touch
// button). A tap fires when the button is let go, a hold fires once while it
// is still held, so a hold is never also a tap. Times are millis() values.
// No Arduino code here, so the UI preview can test it on a PC.
class PressGesture {
public:
    enum Event { NONE, DOWN, TAP, HOLD };

    explicit PressGesture(uint32_t holdMs) : holdMs(holdMs) {}

    // Call with the button's state on every loop
    Event update(bool pressed, uint32_t nowMs) {
        if (!down) {
            if (!pressed) {
                return NONE;
            }
            down = true;
            holdSent = false;
            downAt = nowMs;
            return DOWN;
        }
        if (!pressed) {
            down = false;
            return holdSent ? NONE : TAP;
        }
        if (!holdSent && nowMs - downAt >= holdMs) {  // Also right when millis() wraps around
            holdSent = true;
            return HOLD;
        }
        return NONE;
    }

    bool isDown() const { return down; }

private:
    uint32_t holdMs;
    bool down = false;
    bool holdSent = false;
    uint32_t downAt = 0;
};
