#include "CalendarPage.h"
#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/DateUtil.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

using namespace Components;
using namespace CalendarDate;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

static const int16_t RIGHT = SCREEN_W - MARGIN;
static const int16_t TITLE_Y = 132;  // "October 2026", "TODAY", "WEEK 41"

// Month grid, left half
static const int16_t GRID_X = 54;          // Center of the Monday column
static const int16_t GRID_STEP_X = 46;
static const int16_t WEEKDAY_Y = 168;      // "M T W ..."
static const int16_t GRID_Y = 205;         // Baseline of the first week
static const int16_t GRID_STEP_Y = 34;
static const int16_t DIVIDER_X = 389;

// Event list, right half
static const int16_t LIST_X = 414;
static const int16_t LIST_CENTER_X = (DIVIDER_X + RIGHT) / 2 + 12;
static const int16_t ROW_Y = 180;          // Middle of the first row
static const int16_t ROW_STEP = 43;
static const int ROWS = 5;
static const int16_t TIME_X = 420;
static const int16_t EVENT_TITLE_X = 492;

// Week view
static const int16_t COLUMN_W = (RIGHT - MARGIN) / 7;  // 105
static const int16_t DAY_NAME_Y = 171;
static const int16_t CARD_Y = 193;
static const int16_t CARD_H = 49;
static const int16_t CARD_STEP = 55;
static const int CARDS = 3;
static const int16_t COLUMN_BOTTOM = 390;

// ---------- Text helpers ----------

static uint16_t textWidth(Adafruit_GFX& gfx, const char* text, const GFXfont* font) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.setFont(font);
    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w + (x1 > 0 ? x1 : 0);
}

// Shortens text with ".." until it is at most maxWidth pixels wide
static void fitText(Adafruit_GFX& gfx, char* text, const GFXfont* font, int16_t maxWidth) {
    size_t length = strlen(text);
    if (textWidth(gfx, text, font) <= maxWidth) {
        return;
    }
    while (length > 0) {
        length--;
        while (length > 0 && text[length - 1] == ' ') {
            length--;
        }
        strcpy(text + length, "..");
        if (textWidth(gfx, text, font) <= maxWidth) {
            return;
        }
    }
}

// Keeps whole words that fit, cuts the first word if even that is too long
static void fitWords(Adafruit_GFX& gfx, char* text, const GFXfont* font, int16_t maxWidth) {
    while (textWidth(gfx, text, font) > maxWidth) {
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

// "09:00", or "All day"
static void formatEventTime(char* buffer, size_t size, const CalendarEvent& event) {
    if (event.allDay) {
        snprintf(buffer, size, "All day");
    } else {
        snprintf(buffer, size, "%02d:%02d", event.startMinute / 60, event.startMinute % 60);
    }
}

// "Wed 07 Oct"
static void formatDay(char* buffer, size_t size, int32_t day) {
    int year, month, dayOfMonth;
    toCivil(day, &year, &month, &dayOfMonth);
    snprintf(buffer, size, "%s %02d %s", weekdayShortName(weekdayOf(day)), dayOfMonth,
             monthShortName(month));
}

static void drawDottedColumn(Adafruit_GFX& gfx, int16_t x, int16_t y0, int16_t y1) {
    for (int16_t y = y0; y <= y1; y += 4) {
        gfx.drawPixel(x, y, COLOR_FG);
    }
}

static void drawFreeDayIcon(Adafruit_GFX& gfx, int16_t cx, int16_t top) {
    int16_t x = cx - 23;
    drawCalendarIcon(gfx, x, top);
    for (int16_t i = -1; i <= 1; i++) {
        gfx.drawLine(x + 13, top + 34 + i, x + 20, top + 41 + i, COLOR_FG);
        gfx.drawLine(x + 20, top + 41 + i, x + 33, top + 28 + i, COLOR_FG);
    }
}

// ---------- Day view: month grid ----------

static void drawMonth(Adafruit_GFX& gfx, const CalendarData& calendar, int32_t today,
                      int32_t selected) {
    static const char* const LETTERS[7] = {"M", "T", "W", "T", "F", "S", "S"};
    char text[24];

    int year, month, dayOfMonth;
    toCivil(selected, &year, &month, &dayOfMonth);
    snprintf(text, sizeof(text), "%s %d", monthName(month), year);
    drawText(gfx, MARGIN, TITLE_Y, text, FONT_BODY);

    for (int i = 0; i < 7; i++) {
        drawTextCentered(gfx, GRID_X + i * GRID_STEP_X, WEEKDAY_Y, LETTERS[i], FONT_SMALL);
    }

    int32_t first = fromCivil(year, month, 1);
    int startColumn = weekdayOf(first);

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
        if (calendar.firstOn(day) >= 0) {
            gfx.fillCircle(cx, baseline + 8, 2, color);
        }
    }

    gfx.fillRect(DIVIDER_X, 118, 2, 263, COLOR_FG);
}

// ---------- Day view: event list ----------

static void drawEventRow(Adafruit_GFX& gfx, int16_t middle, const CalendarEvent& event,
                         bool highlighted) {
    char text[48];
    uint16_t color = highlighted ? COLOR_BG : COLOR_FG;

    if (highlighted) {
        gfx.fillRoundRect(LIST_X - 7, middle - 18, RIGHT - LIST_X + 7, 40, 4, COLOR_FG);
    }

    formatEventTime(text, sizeof(text), event);
    drawText(gfx, TIME_X, middle + 8, text, FONT_SMALL_BOLD, color);

    strcpy(text, event.title);
    fitText(gfx, text, FONT_BODY, RIGHT - EVENT_TITLE_X - 8);
    drawText(gfx, EVENT_TITLE_X, middle + 8, text, FONT_BODY, color);
}

// No events: icon, message and the next event after this day
static void drawFreeDay(Adafruit_GFX& gfx, const CalendarData& calendar, bool isToday,
                        int32_t selected) {
    char text[80];
    char day[16];

    drawFreeDayIcon(gfx, LIST_CENTER_X, 160);
    drawTextCentered(gfx, LIST_CENTER_X, 261, isToday ? "Your day is free" : "Nothing planned",
                     FONT_BODY);

    drawTextCentered(gfx, LIST_CENTER_X, 313, "NEXT EVENT", FONT_SMALL);
    const CalendarEvent* next = calendar.nextAfter(selected, 24 * 60);
    if (next == nullptr) {
        snprintf(text, sizeof(text), "No upcoming events");
    } else {
        char time[8];
        formatDay(day, sizeof(day), next->day);
        formatEventTime(time, sizeof(time), *next);
        snprintf(text, sizeof(text), "%s, %s %s", day, time, next->title);
    }
    fitText(gfx, text, FONT_SMALL_BOLD, RIGHT - DIVIDER_X - 24);
    drawTextCentered(gfx, LIST_CENTER_X, 341, text, FONT_SMALL_BOLD);
}

static void drawDayEvents(Adafruit_GFX& gfx, const AppData& data, int32_t today,
                          int32_t selected) {
    const CalendarData& calendar = data.calendar;
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

    if (!calendar.valid) {
        drawTextCentered(gfx, LIST_CENTER_X, 250,
                         data.wifiConnected ? "Loading calendar..." : "No Wi-Fi, no calendar",
                         FONT_BODY);
        return;
    }

    int count = calendar.countOn(selected);
    snprintf(text, sizeof(text), count == 1 ? "%d event" : "%d events", count);
    drawTextRight(gfx, RIGHT, TITLE_Y, text, FONT_SMALL);

    if (count == 0) {
        drawFreeDay(gfx, calendar, selected == today, selected);
        return;
    }

    // Today: highlight the event running now, or else the next one
    int first = calendar.firstOn(selected);
    int highlight = -1;
    if (selected == today) {
        int minute = data.now.tm_hour * 60 + data.now.tm_min;
        for (int i = 0; i < count; i++) {
            if (calendar.events[first + i].endMinute > minute) {



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

    // With more than 5 events the last row says "+N more",
    // and the list scrolls so the highlighted event stays visible
    int slots = count <= ROWS ? count : ROWS - 1;
    int start = 0;
    if (highlight >= slots) {
        start = highlight - slots + 1;
    }

    for (int i = 0; i < slots; i++) {
        drawEventRow(gfx, ROW_Y + i * ROW_STEP, calendar.events[first + start + i],
                     start + i == highlight);
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

    formatEventTime(text, sizeof(text), event);
    drawText(gfx, x + 8, y + 15, text, FONT_SMALL);

    strcpy(text, event.title);
    fitWords(gfx, text, FONT_SMALL_BOLD, w - 14);
    drawText(gfx, x + 8, y + 38, text, FONT_SMALL_BOLD);
}

static void drawWeek(Adafruit_GFX& gfx, const AppData& data, int32_t today, int32_t selected) {
    const CalendarData& calendar = data.calendar;
    char text[40];

    int32_t monday = selected - weekdayOf(selected);
    int32_t sunday = monday + 6;

    snprintf(text, sizeof(text), "WEEK %d", isoWeek(monday));
    drawText(gfx, MARGIN, TITLE_Y, text, FONT_SMALL);

    // "5 to 11 Oct 2026", "28 Sep to 4 Oct 2026", "28 Dec 2026 to 3 Jan 2027"
    int y0, m0, d0, y1, m1, d1;
    toCivil(monday, &y0, &m0, &d0);
    toCivil(sunday, &y1, &m1, &d1);
    if (y0 != y1) {
        snprintf(text, sizeof(text), "%d %s %d to %d %s %d", d0, monthShortName(m0), y0, d1,
                 monthShortName(m1), y1);
    } else if (m0 != m1) {
        snprintf(text, sizeof(text), "%d %s to %d %s %d", d0, monthShortName(m0), d1,
                 monthShortName(m1), y1);
    } else {
        snprintf(text, sizeof(text), "%d to %d %s %d", d0, d1, monthShortName(m1), y1);
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
        toCivil(day, &year, &month, &dayOfMonth);
        snprintf(text, sizeof(text), "%s %d", weekdayShortName(column), dayOfMonth);
        toUpper(text);
        if (day == today) {
            gfx.fillRoundRect(x + 2, DAY_NAME_Y - 20, COLUMN_W - 5, 32, 4, COLOR_FG);
            color = COLOR_BG;
        }
        drawTextCentered(gfx, cx, DAY_NAME_Y, text, FONT_SMALL_BOLD, color);

        if (!calendar.valid) {
            continue;
        }

        int count = calendar.countOn(day);
        if (count == 0) {
            drawTextCentered(gfx, cx, 219, "Free", FONT_SMALL);
            continue;
        }

        int first = calendar.firstOn(day);
        int shown = count < CARDS ? count : CARDS;
        for (int i = 0; i < shown; i++) {
            drawEventCard(gfx, x + 4, CARD_Y + i * CARD_STEP, calendar.events[first + i]);
        }
        if (count > CARDS) {
            snprintf(text, sizeof(text), "+%d more", count - CARDS);
            drawTextCentered(gfx, cx, 375, text, FONT_SMALL);
        }
    }

    if (!calendar.valid) {
        drawTextCentered(gfx, SCREEN_W / 2, 290,
                         data.wifiConnected ? "Loading calendar..." : "No Wi-Fi, no calendar",
                         FONT_BODY);
    }
}

// ---------- Page ----------

void CalendarPage::next() {
    dayOffset += weekView ? 7 : 1;
    if (dayOffset > MAX_DAYS_AHEAD) {
        dayOffset = 0;  // Back to today
    }
}

void CalendarPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);

    if (!data.timeValid) {
        drawTextCentered(gfx, SCREEN_W / 2, 250, "Waiting for the clock...", FONT_BODY);
    } else {
        int32_t today = fromTm(data.now);
        int32_t selected = today + dayOffset;

        if (weekView) {
            drawWeek(gfx, data, today, selected);
        } else {
            drawMonth(gfx, data.calendar, today, selected);
            drawDayEvents(gfx, data, today, selected);
        }
    }

    if (weekView) {
        drawFooter(gfx, "Turn: mode      Press: next week      Hold: day view");
    } else {
        drawFooter(gfx, "Turn: mode      Press: next day      Hold: week view");
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
