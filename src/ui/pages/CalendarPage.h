#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Month grid with the events of one day, or one week in seven columns.
// Turning the knob moves by a day (a week in the week view), within the
// days that are loaded; pressing switches between the two views.
// Designs: assets/UI-figma-E-ink/16_eink-calendar-month-option 1.png,
//          18_eink-calendar-empty-1280x800.png, 19_eink-calendar-week-*.png
class CalendarPage {
public:
    // Start on today, in the day view
    void open() {
        selectedDay = 0;
        weekView = false;
    }

    // Move by days (weeks in the week view), negative = back
    void move(const AppData& data, int steps);

    void toggleView() { weekView = !weekView; }
    bool isWeekView() const { return weekView; }

    void draw(Adafruit_GFX& gfx, const AppData& data);

private:
    int32_t selectedDay = 0;  // Day number, 0 = today (follows the date)
    bool weekView = false;
};
