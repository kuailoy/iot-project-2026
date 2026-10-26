#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Big clock with a daily quote, shown after a period without input
// Design: assets/UI-figma-E-ink/1_eink-sleep-1280x800.png
class SleepPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
