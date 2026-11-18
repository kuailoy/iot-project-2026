#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Device page (menu > Device): Wi-Fi, battery, firmware, and the way to
// restart or reset the device. Layout like the settings list in
// assets/UI-figma-E-ink/24_eink-settings-1280x800.png
class DevicePage {
public:
    enum Row {
        WIFI,      // Opens the Settings page (address and QR code)
        BATTERY,   // Shown only, can't be chosen
        FIRMWARE,  // Shown only
        RESTART,
        RESET,
        ROW_COUNT
    };

    void draw(Adafruit_GFX& gfx, const AppData& data);

    // Move between the rows that can be chosen, wraps around at both ends
    void move(int steps);
    Row selected() const { return selectedRow; }
    void select(Row row) { selectedRow = row; }

private:
    Row selectedRow = WIFI;
};

// The confirm and "restarting" screens of the Device page.
// Designs: 28_eink-settings-restart-confirm, 29_eink-restarting,
//          30_eink-settings-reset-confirm (assets/UI-figma-E-ink/)
namespace RestartPages {

// "Restart the device?": Turn cancels, Press restarts
void drawRestartConfirm(Adafruit_GFX& gfx, const AppData& data);

// "Reset and unpair?": the knob held for 3 s resets. `filledBoxes` (0..3)
// shows how long it is held so far.
void drawResetConfirm(Adafruit_GFX& gfx, const AppData& data, int filledBoxes);

// Shown while the device restarts (E-Ink keeps it without power).
// resetting: after "Reset and unpair", with the steps of a new device.
void drawRestarting(Adafruit_GFX& gfx, bool resetting);

}
