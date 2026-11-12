#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Settings: how to open the settings web page on the phone (QR code with
// the device's address on the home Wi-Fi). Without Wi-Fi: start the Wi-Fi setup.
// Design idea: assets/UI-figma-E-ink/24_eink-settings-1280x800.png ("More options in the web app")
class SettingsPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
