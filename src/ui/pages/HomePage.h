#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Dashboard screen
// Design: assets/UI-figma-E-ink/2_eink-dashboard-1280x800 (16_10).png
class HomePage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
