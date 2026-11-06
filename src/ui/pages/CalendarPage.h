#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Month grid with the events of one day, or one week in seven columns.
// Designs: assets/UI-figma-E-ink/16_eink-calendar-month-option 1.png,
//          18_eink-calendar-empty-1280x800.png, 19_eink-calendar-week-*.png
class CalendarPage {
public:
    // Pressing goes this many days ahead, then back to today
    static const int MAX_DAYS_AHEAD = 56;

    void draw(Adafruit_GFX& gfx, const AppData& data);

    void showToday() { dayOffset = 0; }
    void next();  // Next day, or next week in the week view
    void toggleView() { weekView = !weekView; }
    bool isWeekView() const { return weekView; }

private:
    bool weekView = false;
    int dayOffset = 0;  // Shown day = today + dayOffset
};
