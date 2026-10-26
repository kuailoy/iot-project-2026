#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Mode switcher screen
// Design: assets/UI-figma-E-ink/9_eink-mode-switcher Turn mode-1280x800.png
class MenuPage {
public:
    enum Item {
        DASHBOARD,
        POMODORO,
        CALENDAR,
        HABITS,
        MUSIC,
        ELECTRICITY,
        SETTINGS,
        ITEM_COUNT
    };

    void draw(Adafruit_GFX& gfx, const AppData& data);

    // Move the selection by encoder steps, wraps around at both ends
    void move(int steps);
    Item selected() const { return selectedItem; }

private:
    Item selectedItem = DASHBOARD;
};
