#include "CalendarPage.h"
#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/DateUtil.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

using namespace Components;
using namespace DateUtil;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t TITLE_Y = 132;  // "October 2026", "TODAY", "WEEK 41"

// Month grid, left half
constexpr int16_t GRID_X = 54;          // Center of the Monday column
constexpr int16_t GRID_STEP_X = 46;
constexpr int16_t WEEKDAY_Y = 168;      // "M T W ..."
constexpr int16_t GRID_Y = 205;         // Baseline of the first week
constexpr int16_t GRID_STEP_Y = 34;
constexpr int16_t DIVIDER_X = 389;

// Event list, right half
constexpr int16_t LIST_X = 414;
constexpr int16_t LIST_CENTER_X = (DIVIDER_X + RIGHT) / 2 + 12;
constexpr int16_t ROW_Y = 180;          // Middle of the first row
constexpr int16_t ROW_STEP = 43;
constexpr int ROWS = 5;                 // More events: "+N more" in the last row
constexpr int16_t TIME_X = 420;
constexpr int16_t EVENT_TITLE_X = 492;

// Week view
constexpr int16_t COLUMN_W = (RIGHT - MARGIN) / 7;  // 105
constexpr int16_t DAY_NAME_Y = 171;
constexpr int16_t CARD_Y = 193;
constexpr int16_t CARD_H = 49;
constexpr int16_t CARD_STEP = 55;
constexpr int CARDS = 3;
constexpr int16_t COLUMN_BOTTOM = 390;

// ---------- Helpers ----------

// Keeps whole words that fit, cuts the first word if even that is too long
static void fitWords(Adafruit_GFX& gfx, char* text, const GFXfont* font, int16_t maxWidth) {
    while (measureText(gfx, text, font) > maxWidth) {
        char* space = strrchr(text, ' ');
        if (space != nullptr) {
            *space = 0;
        } else {
            size_t length = strlen(text);
            if (length <= 1) {
                return;
            }
            text[length - 1] = 0;
        }
    }

    // "Sync," looks broken, drop punctuation left at the end
    size_t length = strlen(text);
    while (length > 1 && strchr(",;:-/", text[length - 1]) != nullptr) {
        text[--length] = 0;
    }
}

static void toUpper(char* text) {
    for (char* c = text; *c; c++) {
        *c = toupper(*c);
    }
}

// "Wed 07 Oct"
static void formatDay(char* buffer, size_t size, int32_t day) {
    int year, month, dayOfMonth;
    civilFromDays(day, year, month, dayOfMonth);
    snprintf(buffer, size, "%s %02d %s", WEEKDAY_SHORT[weekday(day)], dayOfMonth, MONTH_SHORT[month - 1]);
}

// Events of this day are downloaded. Other days are unknown, not free.
static bool isLoaded(const AppData& data, int32_t day) {
    return data.calendarValid && day >= data.calendarFirstDay && day <= data.calendarLastDay;
}

static void drawDottedColumn(Adafruit_GFX& gfx, int16_t x, int16_t y0, int16_t y1) {
    for (int16_t y = y0; y <= y1; y += 4) {
        gfx.drawPixel(x, y, COLOR_FG);
    }
}

// ---------- Day view: month grid ----------

static void drawMonth(Adafruit_GFX& gfx, const AppData& data, int32_t today, int32_t selected) {
    static const char* const LETTERS[7] = {"M", "T", "W", "T", "F", "S", "S"};
    char text[24];

    int year, month, dayOfMonth;
    civilFromDays(selected, year, month, dayOfMonth);
    snprintf(text, sizeof(text), "%s %d", MONTH_NAMES[month - 1], year);
    drawText(gfx, MARGIN, TITLE_Y, text, FONT_BODY);

    for (int i = 0; i < 7; i++) {
        drawTextCentered(gfx, GRID_X + i * GRID_STEP_X, WEEKDAY_Y, LETTERS[i], FONT_SMALL);
    }

    int32_t first = daysFromCivil(year, month, 1);
    int startColumn = weekday(first);

    for (int i = 0; i < daysInMonth(year, month); i++) {
        int32_t day = first + i;
        int cell = startColumn + i;
        int16_t cx = GRID_X + (cell % 7) * GRID_STEP_X;
        int16_t baseline = GRID_Y + (cell / 7) * GRID_STEP_Y;
        uint16_t color = COLOR_FG;

        if (day == selected) {
            gfx.fillRoundRect(cx - 18, baseline - 16, 37, 27, 4, COLOR_FG);
            color = COLOR_BG;
        } else if (day == today) {
            gfx.drawRoundRect(cx - 18, baseline - 16, 37, 27, 4, COLOR_FG);
            gfx.drawRoundRect(cx - 17, baseline - 15, 35, 25, 3, COLOR_FG);
        }

        snprintf(text, sizeof(text), "%d", i + 1);
        drawTextCentered(gfx, cx, baseline, text, FONT_SMALL_BOLD, color);

        // Dot under the number: this day has events
        if (isLoaded(data, day) && CalendarData::hasEvents(data, day)) {
            gfx.fillCircle(cx, baseline + 8, 2, color);
        }
    }

    gfx.fillRect(DIVIDER_X, 118, 2, 263, COLOR_FG);
}

// ---------- Day view: event list ----------

static void drawEventRow(Adafruit_GFX& gfx, int16_t middle, const CalendarEvent& event, bool highlighted) {
    char text[48];
    uint16_t color = highlighted ? COLOR_BG : COLOR_FG;

    if (highlighted) {
        gfx.fillRoundRect(LIST_X - 7, middle - 18, RIGHT - LIST_X + 7, 40, 4, COLOR_FG);
    }

    CalendarData::formatTime(text, sizeof(text), event);
    drawText(gfx, TIME_X, middle + 8, text, FONT_SMALL_BOLD, color);

    strlcpy(text, event.title, sizeof(text));
    fitText(gfx, text, RIGHT - EVENT_TITLE_X - 8, FONT_BODY);
    drawText(gfx, EVENT_TITLE_X, middle + 8, text, FONT_BODY, color);
}

// No events: icon, message and the next event after this day
static void drawFreeDay(Adafruit_GFX& gfx, const AppData& data, int32_t today, int32_t selected) {
    char text[80];

    drawCalendarIcon(gfx, LIST_CENTER_X - 23, 160, true);
    drawTextCentered(gfx, LIST_CENTER_X, 261, selected == today ? "Your day is free" : "Nothing planned",
                     FONT_BODY);

    drawTextCentered(gfx, LIST_CENTER_X, 313, "NEXT EVENT", FONT_SMALL);
    const CalendarEvent* next = CalendarData::nextEvent(data, selected, 24 * 60);
    if (next != nullptr) {
        CalendarData::formatNext(text, sizeof(text), *next, today);
    } else {
        snprintf(text, sizeof(text), "No upcoming events");
    }
    fitText(gfx, text, RIGHT - DIVIDER_X - 24, FONT_SMALL_BOLD);
    drawTextCentered(gfx, LIST_CENTER_X, 341, text, FONT_SMALL_BOLD);
}

static void drawDayEvents(Adafruit_GFX& gfx, const AppData& data, int32_t today, int32_t selected) {
    char text[24];

    // "TODAY", "TOMORROW" or "WED 07 OCT"
    if (selected == today) {
        snprintf(text, sizeof(text), "TODAY");
    } else if (selected == today + 1) {
        snprintf(text, sizeof(text), "TOMORROW");
    } else {
        formatDay(text, sizeof(text), selected);
        toUpper(text);
    }
    drawText(gfx, LIST_X, TITLE_Y, text, FONT_SMALL);

    if (!data.calendarValid) {
        drawTextCentered(gfx, LIST_CENTER_X, 250,
                         data.wifiConnected ? "Loading calendar..." : "No Wi-Fi, no calendar", FONT_BODY);
        return;
    }
    if (!isLoaded(data, selected)) {
        // Later days are cut when the feed has more events than fit (completeUntil in IcsParser.h)
        bool cut = selected > data.calendarLastDay;
        drawTextCentered(gfx, LIST_CENTER_X, 250, cut ? "Too many events" : "Not loaded", FONT_BODY);
        drawTextCentered(gfx, LIST_CENTER_X, 284,
                         cut ? "This day could not be loaded" : "Only this week and the next 8 are loaded",
                         FONT_SMALL);
        return;
    }

    int first;
    int count = CalendarData::eventsOnDay(data, selected, &first);
    snprintf(text, sizeof(text), count == 1 ? "%d event" : "%d events", count);
    drawTextRight(gfx, RIGHT, TITLE_Y, text, FONT_SMALL);

    if (count == 0) {
        drawFreeDay(gfx, data, today, selected);
        return;
    }

    // Today: highlight the event running now, or else the next one
    int highlight = -1;
    if (selected == today) {
        int minute = minuteOf(data.now);
        for (int i = 0; i < count; i++) {
            const CalendarEvent& e = data.events[first + i];
            if (!e.allDay && e.endMin > minute) {
                highlight = i;
                break;
            }
        }
    }

    // With more than 5 events the last row says "+N more",
    // and the list scrolls so the highlighted event stays visible
    int slots = count <= ROWS ? count : ROWS - 1;
    int start = 0;
    if (highlight >= slots) {
        start = highlight - slots + 1;
    }

    for (int i = 0; i < slots; i++) {
        drawEventRow(gfx, ROW_Y + i * ROW_STEP, data.events[first + start + i], start + i == highlight);
    }

    int hidden = count - start - slots;
    if (hidden > 0) {
        snprintf(text, sizeof(text), "+%d more", hidden);
        drawText(gfx, TIME_X, ROW_Y + slots * ROW_STEP + 6, text, FONT_SMALL);
    }
}

// ---------- Week view ----------

static void drawEventCard(Adafruit_GFX& gfx, int16_t x, int16_t y, const CalendarEvent& event) {
    char text[40];
    const int16_t w = COLUMN_W - 9;

    gfx.drawRoundRect(x, y, w, CARD_H, 6, COLOR_FG);
    gfx.drawRoundRect(x + 1, y + 1, w - 2, CARD_H - 2, 5, COLOR_FG);

    CalendarData::formatTime(text, sizeof(text), event);
    drawText(gfx, x + 8, y + 15, text, FONT_SMALL);

    strlcpy(text, event.title, sizeof(text));
    fitWords(gfx, text, FONT_SMALL_BOLD, w - 14);
    drawText(gfx, x + 8, y + 38, text, FONT_SMALL_BOLD);
}

static void drawWeek(Adafruit_GFX& gfx, const AppData& data, int32_t today, int32_t selected) {
    char text[40];

    int32_t monday = selected - weekday(selected);
    int32_t sunday = monday + 6;

    snprintf(text, sizeof(text), "WEEK %d", isoWeek(monday));
    drawText(gfx, MARGIN, TITLE_Y, text, FONT_SMALL);

    // "5 to 11 Oct 2026", "28 Sep to 4 Oct 2026", "28 Dec 2026 to 3 Jan 2027"
    int y0, m0, d0, y1, m1, d1;
    civilFromDays(monday, y0, m0, d0);
    civilFromDays(sunday, y1, m1, d1);
    if (y0 != y1) {
        snprintf(text, sizeof(text), "%d %s %d to %d %s %d", d0, MONTH_SHORT[m0 - 1], y0, d1, MONTH_SHORT[m1 - 1],
                 y1);
    } else if (m0 != m1) {
        snprintf(text, sizeof(text), "%d %s to %d %s %d", d0, MONTH_SHORT[m0 - 1], d1, MONTH_SHORT[m1 - 1], y1);
    } else {
        snprintf(text, sizeof(text), "%d to %d %s %d", d0, d1, MONTH_SHORT[m1 - 1], y1);
    }
    drawTextRight(gfx, RIGHT, TITLE_Y, text, FONT_SMALL);

    for (int column = 0; column < 7; column++) {
        int32_t day = monday + column;
        int16_t x = MARGIN + column * COLUMN_W;
        int16_t cx = x + COLUMN_W / 2;
        uint16_t color = COLOR_FG;

        if (column > 0) {
            drawDottedColumn(gfx, x, DAY_NAME_Y - 18, COLUMN_BOTTOM);
        }

        int year, month, dayOfMonth;
        civilFromDays(day, year, month, dayOfMonth);
        snprintf(text, sizeof(text), "%s %d", WEEKDAY_SHORT[column], dayOfMonth);
        toUpper(text);
        if (day == today) {
            gfx.fillRoundRect(x + 2, DAY_NAME_Y - 20, COLUMN_W - 5, 32, 4, COLOR_FG);
            color = COLOR_BG;
        }
        drawTextCentered(gfx, cx, DAY_NAME_Y, text, FONT_SMALL_BOLD, color);

        if (!data.calendarValid) {
            continue;
        }
        if (!isLoaded(data, day)) {
            drawTextCentered(gfx, cx, 219, "--", FONT_SMALL);  // Unknown, not free
            continue;
        }

        int first;
        int count = CalendarData::eventsOnDay(data, day, &first);
        if (count == 0) {
            drawTextCentered(gfx, cx, 219, "Free", FONT_SMALL);
            continue;
        }

        int shown = count < CARDS ? count : CARDS;
        for (int i = 0; i < shown; i++) {
            drawEventCard(gfx, x + 4, CARD_Y + i * CARD_STEP, data.events[first + i]);
        }
        if (count > CARDS) {
            snprintf(text, sizeof(text), "+%d more", count - CARDS);
            drawTextCentered(gfx, cx, 375, text, FONT_SMALL);
        }
    }

    if (!data.calendarValid) {
        drawTextCentered(gfx, SCREEN_W / 2, 290,
                         data.wifiConnected ? "Loading calendar..." : "No Wi-Fi, no calendar", FONT_BODY);
    }
}

// ---------- Page ----------

void CalendarPage::move(const AppData& data, int steps) {
    if (!data.timeValid) {
        return;
    }
    int32_t today = dayOf(data.now);
    int32_t day = (selectedDay != 0 ? selectedDay : today) + steps * (weekView ? 7 : 1);

    // Only days with loaded events
    int32_t firstDay = data.calendarValid ? data.calendarFirstDay : today;
    int32_t lastDay = data.calendarValid ? data.calendarLastDay : today + CALENDAR_DAYS_AHEAD;
    if (day > lastDay) {
        day = lastDay;
    }
    if (day < firstDay) {
        day = firstDay;
    }
    selectedDay = day == today ? 0 : day;
}

void CalendarPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);
    drawFooter(gfx, weekView ? "Turn: week      Press: day view      Hold: back"
                             : "Turn: day      Press: week view      Hold: back");

    if (!data.timeValid) {
        drawTextCentered(gfx, SCREEN_W / 2, 250, "Waiting for the clock to sync", FONT_BODY);
        return;
    }

    int32_t today = dayOf(data.now);
    int32_t selected = selectedDay != 0 ? selectedDay : today;
    if (weekView) {
        drawWeek(gfx, data, today, selected);
    } else {
        drawMonth(gfx, data, today, selected);
        drawDayEvents(gfx, data, today, selected);
    }
}
