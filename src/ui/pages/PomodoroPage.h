#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"
#include "../../app/Pomodoro.h"

// Pomodoro timer, one layout for every timer state.
// Only whole minutes are shown, so the page changes at most once a minute.
// Designs: assets/UI-figma-E-ink/10_ to 15_eink-pomodoro-*.png
class PomodoroPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data, const Pomodoro& timer, uint32_t nowMs);
};
