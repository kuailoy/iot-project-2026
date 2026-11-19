#pragma once

#include <Adafruit_GFX.h>
#include <time.h>

// Shown when the device turns off (hold the touch button on a home page).
// E-Ink keeps the image without power, so it stays while the device is off.
// Layout like assets/UI-figma-E-ink/26_eink-battery-empty-1280x800-trmnl-font 1.png
class PowerOffPage {
public:
    // turnedOffAt = local time when it was turned off, nullptr if the clock wasn't synced
    void draw(Adafruit_GFX& gfx, const struct tm* turnedOffAt);
};
