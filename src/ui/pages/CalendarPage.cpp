#include "CalendarPage.h"

#include <stdio.h>
#include <string.h>

#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/DateUtil.h"

using namespace Components;
using namespace DateUtil;

// ---------- Layout ----------

constexpr int16_t TITLE_Y = 134;        // "Today, Wednesday 7 October"
constexpr int16_t LIST_FIRST_Y = 186;   // Baseline of the first event
constexpr int16_t LIST_STEP_Y = 43;
constexpr int LIST_ROWS = 5;            // More events: "+N more" in the last row
constexpr int16_t TIME_X = MARGIN + 12;
constexpr int16_t TITLE_X = MARGIN + 150;
constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t LOCATION_MAX_W = 180;  // Longer locations end with ".."

void CalendarPage::move(const AppData& data, int steps) {
    if (!data.timeValid) {
        return;
    }
    int32_t today = dayOf(data.now);
    int32_t day = (selectedDay != 0 ? selectedDay : today) + steps;

    // Only days with loaded events
    int32_t firstDay = data.calendarValid ? data.calendarFirstDay : today;
    int32_t lastDay = data.calendarValid ? data.calendarLastDay : today + CALENDAR_DAYS_AHEAD;
    if (day < firstDay) {
        day = firstDay;
    }
    if (day > lastDay) {
        day = lastDay;
    }
    selectedDay = day;
}

void CalendarPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    char text[64];
    drawHeader(gfx, data);
    drawFooter(gfx, "Turn: day      Press: menu      Hold: back");

    if (!data.timeValid) {
        drawTextCentered(gfx, SCREEN_W / 2, 250, "Waiting for the clock to sync", FONT_BODY);
        return;
    }

    int32_t today = dayOf(data.now);
    int32_t day = selectedDay != 0 ? selectedDay : today;

    // "Today, Wednesday 7 October" / "Tomorrow, ..." / "Friday 9 October"
    static const char* const WEEKDAY_NAMES[7] = {"Monday", "Tuesday", "Wednesday", "Thursday",
                                                 "Friday", "Saturday", "Sunday"};
    int year, month, dayOfMonth;
    civilFromDays(day, year, month, dayOfMonth);
    const char* prefix = day == today ? "Today, " : day == today + 1 ? "Tomorrow, " : day == today - 1 ? "Yesterday, " : "";
    snprintf(text, sizeof(text), "%s%s %d %s", prefix, WEEKDAY_NAMES[weekday(day)], dayOfMonth,
             MONTH_NAMES[month - 1]);
    drawText(gfx, MARGIN, TITLE_Y, text, FONT_BODY);

    bool loaded = data.calendarValid && day >= data.calendarFirstDay && day <= data.calendarLastDay;
    if (!loaded) {
        drawTextCentered(gfx, SCREEN_W / 2, 260, "Calendar not synced yet", FONT_BODY);
        return;
    }

    int first;
    int count = CalendarData::eventsOnDay(data, day, &first);
    snprintf(text, sizeof(text), "%d event%s", count, count == 1 ? "" : "s");
    drawTextRight(gfx, RIGHT, TITLE_Y, text, FONT_SMALL);

    if (count == 0) {
        drawFreeDay(gfx, today, day, data);
        return;
    }

    // Today: highlight the event in progress, or else the next one
    int highlight = -1;
    if (day == today) {
        int minute = minuteOf(data.now);
        for (int i = first; i < first + count; i++) {
            if (!data.events[i].allDay && data.events[i].endMin > minute) {
                highlight = i;
                break;
            }
        }
    }

    int shown = count > LIST_ROWS ? LIST_ROWS - 1 : count;
    for (int i = 0; i < shown; i++) {
        const CalendarEvent& e = data.events[first + i];
        int16_t baseline = LIST_FIRST_Y + i * LIST_STEP_Y;
        uint16_t color = COLOR_FG;
        if (first + i == highlight) {
            gfx.fillRoundRect(MARGIN, baseline - 28, RIGHT - MARGIN, 40, 4, COLOR_FG);
            color = COLOR_BG;
        } else if (i < shown - 1) {
            drawDottedLine(gfx, MARGIN, RIGHT, baseline + 14);
        }

        // "09:00 - 10:30", or "All day"
        if (e.allDay) {
            snprintf(text, sizeof(text), "All day");
        } else {
            int end = e.endMin >= 1440 ? 1439 : e.endMin;
            snprintf(text, sizeof(text), "%02d:%02d - %02d:%02d", e.startMin / 60, e.startMin % 60, end / 60,
                     end % 60);
        }
        drawText(gfx, TIME_X, baseline, text, FONT_SMALL_BOLD, color);

        // Location on the right, the title gets the space that is left
        int16_t titleRight = RIGHT - 12;
        if (e.location[0] != '\0') {
            strlcpy(text, e.location, sizeof(text));
            fitText(gfx, text, LOCATION_MAX_W, FONT_SMALL);
            drawTextRight(gfx, RIGHT - 12, baseline, text, FONT_SMALL, color);
            titleRight -= measureText(gfx, text, FONT_SMALL) + 20;
        }

        strlcpy(text, e.title, sizeof(text));
        fitText(gfx, text, titleRight - TITLE_X, FONT_BODY);
        drawText(gfx, TITLE_X, baseline, text, FONT_BODY, color);
    }

    if (count > shown) {
        snprintf(text, sizeof(text), "+%d more", count - shown);
        drawText(gfx, TIME_X, LIST_FIRST_Y + shown * LIST_STEP_Y, text, FONT_SMALL_BOLD);
    }
}

// No events: icon, message and the next event after this day
void CalendarPage::drawFreeDay(Adafruit_GFX& gfx, int32_t today, int32_t day, const AppData& data) {
    char text[64];
    const int16_t cx = SCREEN_W / 2;

    drawCalendarIcon(gfx, cx - 23, 160, true);
    drawTextCentered(gfx, cx, 256, "Your day is free", FONT_BODY);
    drawTextCentered(gfx, cx, 306, "NEXT EVENT", FONT_SMALL);

    int minute = day == today ? minuteOf(data.now) : 0;
    const CalendarEvent* next = CalendarData::nextEvent(data, day, minute);
    if (next != nullptr) {
        CalendarData::formatNext(text, sizeof(text), *next, today);
    } else {
        snprintf(text, sizeof(text), "No upcoming events");
    }
    fitText(gfx, text, RIGHT - MARGIN, FONT_SMALL_BOLD);
    drawTextCentered(gfx, cx, 338, text, FONT_SMALL_BOLD);
}
