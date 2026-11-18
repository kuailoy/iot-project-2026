// Knob and drawing for the Device page: restart and reset.
// Part of the UI class, kept apart from UI.cpp; UI.cpp calls these first.

#include "UI.h"

#include "Components.h"

#include <Arduino.h>

namespace {

constexpr unsigned long RESET_HOLD_MS = 3000;  // "Hold 3s: reset"

}  // namespace

void UI::setKnobDown(bool down) {
    if (down && !knobDown) {
        knobDownSinceMs = millis();
    }
    knobDown = down;
}

bool UI::readyToRestart() const {
    return restartPending && currentPage == RESTARTING && restartingDrawn && display != nullptr &&
           !display->isBusy();
}

// Boxes filled on the reset page: one per second the knob has been held
// since the page opened (the press that opened it doesn't count)
int UI::resetBoxes() const {
    bool holding = knobDown && int32_t(knobDownSinceMs - resetPageOpenedMs) >= 0;
    if (!holding) {
        return 0;
    }
    unsigned long held = millis() - knobDownSinceMs;
    return held >= RESET_HOLD_MS ? 3 : int(held / 1000);
}

bool UI::rotateDevice(int steps) {
    switch (currentPage) {
        case DEVICE:
            devicePage.move(steps);
            redrawNeeded = true;
            return true;
        case RESTART_CONFIRM:
        case RESET_CONFIRM:
            openPage(DEVICE);  // "Turn: cancel"
            return true;
        case RESTARTING:
            return true;  // Nothing to do, it restarts
        default:
            return false;
    }
}

bool UI::pressDevice() {
    switch (currentPage) {
        case DEVICE:
            switch (devicePage.selected()) {
                case DevicePage::WIFI:
                    settingsOpened = true;  // main.cpp fills in the address and QR code
                    openPage(SETTINGS);
                    break;
                case DevicePage::RESTART:
                    openPage(RESTART_CONFIRM);
                    break;
                case DevicePage::RESET:
                    openPage(RESET_CONFIRM);
                    break;
                default:
                    break;
            }
            return true;
        case RESTART_CONFIRM:
            // "Press: restart": main.cpp restarts once the page is on the screen
            resetting = false;
            restartPending = true;
            openPage(RESTARTING);
            return true;
        case RESET_CONFIRM:
        case RESTARTING:
            return true;  // A short press doesn't reset: only holding for 3 s does
        default:
            return false;
    }
}

bool UI::longPressDevice() {
    switch (currentPage) {
        case RESET_CONFIRM:
        case RESTART_CONFIRM:
        case RESTARTING:
            return true;  // Held on the reset page: updateDevice() counts the time
        default:
            return false;  // Device page: "Hold: back" like the other pages
    }
}

void UI::updateDevice() {
    if (currentPage == RESET_CONFIRM) {
        int boxes = resetBoxes();
        if (boxes >= 3) {
            resetting = true;  // main.cpp erases everything, then restarts
            restartPending = true;
            openPage(RESTARTING);
        } else if (boxes != resetBoxesDrawn) {
            redrawNeeded = true;  // Fill the boxes as it is held, empty them when let go
        }
    }

    if (currentPage == RESTARTING && restartingDrawn) {
        redrawNeeded = false;  // Stays until the restart: no refresh at the next minute
    }
}

bool UI::drawDevice() {
    switch (currentPage) {
        case DEVICE:
            devicePage.draw(*display, *data);
            return true;
        case RESTART_CONFIRM:
            RestartPages::drawRestartConfirm(*display, *data);
            return true;
        case RESET_CONFIRM:
            resetBoxesDrawn = resetBoxes();
            RestartPages::drawResetConfirm(*display, *data, resetBoxesDrawn);
            return true;
        case RESTARTING:
            RestartPages::drawRestarting(*display, resetting);
            restartingDrawn = true;
            return true;
        default:
            return false;
    }
}
