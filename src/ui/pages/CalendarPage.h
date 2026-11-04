#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Calendar: the events of one day, today first. Turning the knob moves to
// the next or previous day.
// Designs: the "TODAY" list of assets/UI-figma-E-ink/16_eink-calendar-month-option 1.png
// and assets/UI-figma-E-ink/18_eink-calendar-empty-1280x800.png, full width.
class CalendarPage {
public:
    // Start on today
    void open() { selectedDay = 0; }

    // Move by days, negative = back. Stays within the days that are loaded.
    void move(const AppData& data, int steps);

    void draw(Adafruit_GFX& gfx, const AppData& data);

private:
    int32_t selectedDay = 0;  // Day number, 0 = today

    void drawFreeDay(Adafruit_GFX& gfx, int32_t today, int32_t day, const AppData& data);
};
