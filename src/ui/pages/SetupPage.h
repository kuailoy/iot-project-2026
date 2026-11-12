#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Wi-Fi setup: QR code to join the setup Wi-Fi and three steps.
// Design: assets/UI-figma-E-ink/3_eink-setup-1-wifi-1280x800 1.png
class SetupPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
