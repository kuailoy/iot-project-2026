#include "WidgetPage.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/DateUtil.h"

using namespace Components;
using namespace DateUtil;

// ---------- Grid ----------
// Like the Home page: header on top, footer at the bottom, and 4 x 2 cells
// (192 x 153 px) between their lines. No frames: lines separate the widgets.

constexpr int16_t PAD = 16;                              // Inside a widget
constexpr int16_t GRID_X = MARGIN - PAD;                 // So text starts at the page margin
constexpr int16_t GRID_TOP = HEADER_LINE_Y + 2;
constexpr int16_t CELL_W = (SCREEN_W - 2 * GRID_X) / WIDGET_COLS;
constexpr int16_t CELL_H = (FOOTER_LINE_Y - GRID_TOP) / WIDGET_ROWS;
constexpr int16_t CONTENT_INSET = 6;                     // Space above and below the content
constexpr int16_t DIVIDER_INSET = 20;                    // Vertical lines stop short, as on Home
constexpr int16_t LABEL_Y = 30;

struct Box {
    int16_t x, y, w, h;
    int16_t right() const { return x + w; }
    int16_t cx() const { return x + w / 2; }
};

static void upperCase(char* text) {
    for (; *text; text++) {
        *text = toupper(*text);
    }
}

static void drawLabel(Adafruit_GFX& gfx, const Box& b, const char* label) {
    drawText(gfx, b.x + PAD, b.y + LABEL_Y, label, FONT_SMALL);
}

static void formatTemp(char* text, size_t size, bool valid, float value) {
    if (valid) {
        snprintf(text, size, "%d", (int)lroundf(value));
    } else {
        snprintf(text, size, "--");
    }
}

// Big value with a small caption under it, e.g. "45%" / "Humidity"
static void drawValue(Adafruit_GFX& gfx, int16_t x, int16_t baseline, const char* value, const char* caption,
                      bool degrees) {
    if (degrees) {
        drawTemperature(gfx, x, baseline, value, FONT_TEMP);
    } else {
        drawText(gfx, x, baseline, value, FONT_TEMP);
    }
    drawText(gfx, x, baseline + 28, caption, FONT_SMALL);
}

// ---------- Widgets ----------

static void drawClock(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char time[6] = "--:--";
    char date[32] = "";
    if (data.timeValid) {
        strftime(time, sizeof(time), "%H:%M", &data.now);
        strftime(date, sizeof(date), "%A %d %B", &data.now);  // "Monday 05 October"
    }

    // The big clock font (108 px high) in large and wide widgets, otherwise the 24 pt one
    bool roomy = b.h >= 200 || b.w >= 600;
    bool big = roomy && measureText(gfx, time, FONT_CLOCK) <= b.w - 12;
    if (big && b.w >= 600) {
        // Wide: clock on the left, date on the right
        drawText(gfx, b.x + PAD + 4, b.y + b.h - 17, time, FONT_CLOCK);
        char weekday[12] = "";
        char dayMonth[20] = "";
        if (data.timeValid) {
            strftime(weekday, sizeof(weekday), "%A", &data.now);
            strftime(dayMonth, sizeof(dayMonth), "%d %B", &data.now);
        }
        drawTextRight(gfx, b.right() - PAD - 4, b.y + b.h / 2 - 4, weekday, FONT_BODY);
        drawTextRight(gfx, b.right() - PAD - 4, b.y + b.h / 2 + 26, dayMonth, FONT_SMALL_BOLD);
    } else if (big) {
        int16_t baseline = b.h >= 200 ? b.y + b.h / 2 + 30 : b.y + b.h - 17;
        drawTextCentered(gfx, b.cx(), baseline, time, FONT_CLOCK);
        if (b.h >= 200) {
            upperCase(date);
            fitText(gfx, date, b.w - 2 * PAD, FONT_SMALL_BOLD);
            drawTextCentered(gfx, b.cx(), baseline + 44, date, FONT_SMALL_BOLD);
        }
    } else {
        drawTextCentered(gfx, b.cx(), b.y + b.h / 2 + 8, time, FONT_TEMP);
        if (data.timeValid) {
            strftime(date, sizeof(date), "%a %d %b", &data.now);  // "Mon 05 Oct"
            upperCase(date);
        }
        drawTextCentered(gfx, b.cx(), b.y + b.h / 2 + 40, date, FONT_SMALL_BOLD);
    }
}

static void drawDate(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[32];
    if (!data.timeValid) {
        drawTextCentered(gfx, b.cx(), b.y + b.h / 2 + 12, "--", FONT_TEMP);
        return;
    }
    int32_t today = dayOf(data.now);

    if (b.w < 300) {
        // Small: MON / 5 / OCTOBER, centered
        strftime(text, sizeof(text), "%a", &data.now);
        upperCase(text);
        drawTextCentered(gfx, b.cx(), b.y + 36, text, FONT_SMALL_BOLD);
        snprintf(text, sizeof(text), "%d", data.now.tm_mday);
        drawTextCentered(gfx, b.cx(), b.y + b.h / 2 + 22, text, FONT_TEMP);
        strftime(text, sizeof(text), "%B", &data.now);
        upperCase(text);
        drawTextCentered(gfx, b.cx(), b.y + b.h - 18, text, FONT_SMALL);
        return;
    }

    strftime(text, sizeof(text), "%A", &data.now);
    drawText(gfx, b.x + PAD, b.y + 42, text, FONT_BODY);
    strftime(text, sizeof(text), "%d %B", &data.now);
    drawText(gfx, b.x + PAD, b.y + 92, text[0] == '0' ? text + 1 : text, FONT_TEMP);
    snprintf(text, sizeof(text), "Week %d, %d", isoWeek(today), data.now.tm_year + 1900);
    drawText(gfx, b.x + PAD, b.y + 124, text, FONT_SMALL);
}

static void drawWeather(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[8];
    formatTemp(text, sizeof(text), data.weatherValid, data.outdoorTemp);
    drawLabel(gfx, b, "WEATHER");

    if (b.w < 300) {
        drawTemperature(gfx, b.x + PAD, b.y + 86, text, FONT_TEMP);
        drawText(gfx, b.x + PAD, b.y + 116, data.city, FONT_SMALL);
        return;
    }
    // Cloud on the left, temperature and city on the right
    // TODO: Pick the icon from the weather code (sun, rain, snow...)
    int16_t iconY = b.h >= 200 ? b.y + b.h / 2 - 50 : b.y + 44;
    drawCloudIcon(gfx, b.x + PAD, iconY);
    int16_t textX = b.x + PAD + 160;
    int16_t baseline = b.h >= 200 ? b.y + b.h / 2 + 6 : b.y + 90;
    drawTemperature(gfx, textX, baseline, text, FONT_TEMP);
    drawText(gfx, textX, baseline + 30, data.city, FONT_SMALL);
}

static void co2Level(char* text, size_t size, const AppData& data) {
    if (!data.sensorValid) {
        snprintf(text, size, "--");
    } else if (data.co2 < 800) {
        snprintf(text, size, "Good air");
    } else if (data.co2 < 1200) {
        snprintf(text, size, "OK");
    } else {
        snprintf(text, size, "Open a window");
    }
}

static void drawIndoor(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char temp[8], humidity[8], co2[8];
    formatTemp(temp, sizeof(temp), data.sensorValid, data.indoorTemp);
    if (data.sensorValid) {
        snprintf(humidity, sizeof(humidity), "%d%%", (int)lroundf(data.humidity));
        snprintf(co2, sizeof(co2), "%u", data.co2);
    } else {
        snprintf(humidity, sizeof(humidity), "--%%");
        snprintf(co2, sizeof(co2), "--");
    }
    drawLabel(gfx, b, "INDOOR");

    if (b.w < 300) {
        drawTemperature(gfx, b.x + PAD, b.y + 86, temp, FONT_TEMP);
        char text[16];
        snprintf(text, sizeof(text), "%s RH", humidity);
        drawText(gfx, b.x + PAD, b.y + 116, text, FONT_SMALL);
        return;
    }
    if (b.h >= 200) {
        // Large: three rows
        drawValue(gfx, b.x + PAD, b.y + 84, temp, "Temperature", true);
        drawValue(gfx, b.x + PAD, b.y + 168, humidity, "Humidity", false);
        drawValue(gfx, b.x + PAD, b.y + 252, co2, "CO2 ppm", false);
        return;
    }
    // Medium: two columns, wide: three
    int columns = b.w >= 600 ? 3 : 2;
    int16_t step = (b.w - 2 * PAD) / columns;
    drawValue(gfx, b.x + PAD, b.y + 86, temp, "Temperature", true);
    drawValue(gfx, b.x + PAD + step, b.y + 86, humidity, "Humidity", false);
    if (columns == 3) {
        drawValue(gfx, b.x + PAD + 2 * step, b.y + 86, co2, "CO2 ppm", false);
    }
}

static void drawCo2(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char value[8], level[16];
    if (data.sensorValid) {
        snprintf(value, sizeof(value), "%u", data.co2);
    } else {
        snprintf(value, sizeof(value), "--");
    }
    co2Level(level, sizeof(level), data);
    drawLabel(gfx, b, "CO2");

    int16_t baseline = b.h >= 200 ? b.y + b.h / 2 + 12 : b.y + 86;
    drawText(gfx, b.x + PAD, baseline, value, FONT_TEMP);
    drawText(gfx, gfx.getCursorX() + 8, baseline, "ppm", FONT_SMALL);
    drawText(gfx, b.x + PAD, baseline + 30, level, FONT_SMALL_BOLD);
}

static void drawNextEvent(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[64];
    drawLabel(gfx, b, "NEXT EVENT");

    const CalendarEvent* next = CalendarData::nextEvent(data);
    if (next == nullptr) {
        drawText(gfx, b.x + PAD, b.y + 80, data.calendarValid ? "No upcoming events" : "--", FONT_SMALL_BOLD);
        return;
    }

    // When: "10:30" today, "Wed 07 Oct, 09:00" later
    int32_t today = dayOf(data.now);
    char time[8];
    CalendarData::formatTime(time, sizeof(time), *next);
    if (next->day == today) {
        snprintf(text, sizeof(text), "Today, %s", time);
    } else if (next->day == today + 1) {
        snprintf(text, sizeof(text), "Tomorrow, %s", time);
    } else {
        int year, month, day;
        civilFromDays(next->day, year, month, day);
        snprintf(text, sizeof(text), "%s %d %s, %s", WEEKDAY_SHORT[weekday(next->day)], day, MONTH_SHORT[month - 1],
                 time);
    }
    fitText(gfx, text, b.w - 2 * PAD, FONT_SMALL_BOLD);
    drawText(gfx, b.x + PAD, b.y + 58, text, FONT_SMALL_BOLD);

    const GFXfont* titleFont = b.w < 300 ? FONT_SMALL_BOLD : FONT_BODY;
    strlcpy(text, next->title, sizeof(text));
    fitText(gfx, text, b.w - 2 * PAD, titleFont);
    drawText(gfx, b.x + PAD, b.y + 90, text, titleFont);

    if (next->location[0] != '\0') {
        strlcpy(text, next->location, sizeof(text));
        fitText(gfx, text, b.w - 2 * PAD, FONT_SMALL);
        drawText(gfx, b.x + PAD, b.y + 118, text, FONT_SMALL);
    }
}

static void drawAgenda(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[64];
    drawLabel(gfx, b, "TODAY");
    if (!data.timeValid || !data.calendarValid) {
        drawText(gfx, b.x + PAD, b.y + 72, "--", FONT_SMALL_BOLD);
        return;
    }

    // Events of today that haven't ended yet
    int32_t today = dayOf(data.now);
    int minute = minuteOf(data.now);
    int first;
    int count = CalendarData::eventsOnDay(data, today, &first);
    int remaining = 0;
    for (int i = first; i >= 0 && i < first + count; i++) {
        remaining += data.events[i].allDay || data.events[i].endMin > minute;
    }
    snprintf(text, sizeof(text), "%d left", remaining);
    drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, text, FONT_SMALL);

    if (remaining == 0) {
        drawText(gfx, b.x + PAD, b.y + 72, "Nothing more today", FONT_SMALL_BOLD);
        return;
    }

    const int16_t rowStep = 26;
    int rows = (b.h - 44) / rowStep;
    int shown = 0;
    for (int i = first; i < first + count && shown < rows; i++) {
        const CalendarEvent& e = data.events[i];
        if (!e.allDay && e.endMin <= minute) {
            continue;
        }
        int16_t baseline = b.y + 62 + shown * rowStep;
        if (shown == rows - 1 && remaining > rows) {
            snprintf(text, sizeof(text), "+%d more", remaining - shown);
            drawText(gfx, b.x + PAD, baseline, text, FONT_SMALL_BOLD);
            break;
        }
        CalendarData::formatTime(text, sizeof(text), e);
        drawText(gfx, b.x + PAD, baseline, text, FONT_SMALL_BOLD);
        strlcpy(text, e.title, sizeof(text));
        fitText(gfx, text, b.w - PAD - (PAD + 66), FONT_SMALL);
        drawText(gfx, b.x + PAD + 66, baseline, text, FONT_SMALL);
        shown++;
    }
}

// ---------- Claude plan usage ----------

constexpr time_t CLAUDE_STALE_S = 30 * 60;  // Older numbers are marked as old

// "in 2 h 20 min" (session), "Fri 22:00" (weekly), "" if unknown
static void formatReset(char* text, size_t size, time_t reset, const AppData& data, bool relative) {
    text[0] = '\0';
    if (reset == 0 || data.epoch == 0) {
        return;
    }
    long left = long(reset - data.epoch);
    if (left <= 0) {
        snprintf(text, size, "resets now");
    } else if (relative) {
        long minutes = (left + 59) / 60;
        if (minutes >= 60) {
            snprintf(text, size, "resets in %ld h %ld min", minutes / 60, minutes % 60);
        } else {
            snprintf(text, size, "resets in %ld min", minutes);
        }
    } else {
        struct tm local;
        localtime_r(&reset, &local);
        char when[16];
        strftime(when, sizeof(when), "%a %H:%M", &local);
        snprintf(text, size, "resets %s", when);
    }
}

// Outline with a filled part for the percentage
static void drawBar(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h, int percent) {
    gfx.drawRoundRect(x, y, w, h, h / 2 > 4 ? 4 : h / 2, COLOR_FG);
    int16_t fill = int16_t((long)(w - 4) * (percent > 100 ? 100 : percent) / 100);
    if (fill > 0) {
        gfx.fillRect(x + 2, y + 2, fill, h - 4, COLOR_FG);
    }
}

static void drawClaude(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[48];
    drawLabel(gfx, b, "CLAUDE");

    if (!data.claudeValid) {
        drawText(gfx, b.x + PAD, b.y + 72, "Waiting for data", FONT_SMALL_BOLD);
        if (b.w >= 300) {
            drawText(gfx, b.x + PAD, b.y + 98, "Install the browser extension", FONT_SMALL);
        }
        return;
    }

    // Old numbers: say how old
    if (data.epoch > 0 && data.epoch - data.claudeUpdated > CLAUDE_STALE_S) {
        long minutes = long(data.epoch - data.claudeUpdated) / 60;
        bool small = b.w < 300;  // Short form, next to the label
        if (minutes >= 120) {
            snprintf(text, sizeof(text), small ? "%ldh" : "%ld h ago", minutes / 60);
        } else {
            snprintf(text, sizeof(text), small ? "%ldm" : "%ld min ago", minutes);
        }
        drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, text, FONT_SMALL);
    }

    char sessionReset[32], weeklyReset[32];
    formatReset(sessionReset, sizeof(sessionReset), data.claudeSessionReset, data, true);
    formatReset(weeklyReset, sizeof(weeklyReset), data.claudeWeeklyReset, data, false);
    int16_t barW = b.w - 2 * PAD;

    if (b.w < 300) {
        // Small: the session only
        snprintf(text, sizeof(text), "%u%%", data.claudeSession);
        drawText(gfx, b.x + PAD, b.y + 76, text, FONT_TEMP);
        drawBar(gfx, b.x + PAD, b.y + 86, barW, 12, data.claudeSession);
        snprintf(text, sizeof(text), "Week %u%%", data.claudeWeekly);
        drawText(gfx, b.x + PAD, b.y + 122, text, FONT_SMALL);
        return;
    }

    if (b.h >= 200) {
        // Large: big numbers with a caption and a thick bar each
        struct Row {
            const char* name;
            uint8_t percent;
            const char* reset;
        } rows[2] = {{"session", data.claudeSession, sessionReset}, {"week", data.claudeWeekly, weeklyReset}};
        for (int i = 0; i < 2; i++) {
            int16_t top = b.y + 44 + i * 122;
            snprintf(text, sizeof(text), "%u%%", rows[i].percent);
            drawText(gfx, b.x + PAD, top + 40, text, FONT_TEMP);
            snprintf(text, sizeof(text), "of the %s%s%s", rows[i].name, rows[i].reset[0] ? ", " : "", rows[i].reset);
            fitText(gfx, text, barW, FONT_SMALL);
            drawText(gfx, b.x + PAD, top + 66, text, FONT_SMALL);
            drawBar(gfx, b.x + PAD, top + 76, barW, 16, rows[i].percent);
        }
        return;
    }

    // Medium and wide: two rows, "Session  resets in 2 h 20 min   54%" over a bar
    struct Row {
        const char* name;
        uint8_t percent;
        const char* reset;
    } rows[2] = {{"Session", data.claudeSession, sessionReset}, {"Week", data.claudeWeekly, weeklyReset}};
    for (int i = 0; i < 2; i++) {
        int16_t baseline = b.y + 60 + i * 44;
        drawText(gfx, b.x + PAD, baseline, rows[i].name, FONT_SMALL_BOLD);
        snprintf(text, sizeof(text), "%u%%", rows[i].percent);
        drawTextRight(gfx, b.right() - PAD, baseline, text, FONT_SMALL_BOLD);
        int16_t percentW = measureText(gfx, text, FONT_SMALL_BOLD);
        if (rows[i].reset[0]) {
            char reset[32];
            strlcpy(reset, rows[i].reset, sizeof(reset));
            int16_t x = b.x + PAD + 80;
            fitText(gfx, reset, b.right() - PAD - percentW - 12 - x, FONT_SMALL);
            drawText(gfx, x, baseline, reset, FONT_SMALL);
        }
        drawBar(gfx, b.x + PAD, baseline + 7, barW, 12, rows[i].percent);
    }
}

// ---------- Page ----------

void WidgetPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);
    drawFooter(gfx, "Press: menu      Hold: refresh      Touch: home");

    if (data.widgetCount == 0) {
        int16_t middle = (GRID_TOP + FOOTER_LINE_Y) / 2;
        drawTextCentered(gfx, SCREEN_W / 2, middle - 6, "No widgets yet", FONT_BODY);
        drawTextCentered(gfx, SCREEN_W / 2, middle + 24, "Add them on your phone: Menu > Settings", FONT_SMALL);
        return;
    }

    for (int i = 0; i < data.widgetCount; i++) {
        const WidgetSlot& slot = data.widgets[i];
        int16_t cellX = GRID_X + slot.col * CELL_W;
        int16_t cellY = GRID_TOP + slot.row * CELL_H;
        int16_t cellW = slot.cols * CELL_W;
        int16_t cellH = slot.rows * CELL_H;

        // Lines like on Home: a full-width line between the rows, and a
        // shorter vertical one between widgets side by side
        if (slot.row > 0) {
            int16_t left = slot.col == 0 ? MARGIN : cellX;
            int16_t right = slot.col + slot.cols == WIDGET_COLS ? SCREEN_W - MARGIN : cellX + cellW;
            gfx.fillRect(left, cellY - 1, right - left, 2, COLOR_FG);
        }
        if (slot.col > 0) {
            gfx.fillRect(cellX - 1, cellY + DIVIDER_INSET, 2, cellH - 2 * DIVIDER_INSET, COLOR_FG);
        }

        Box b;
        b.x = cellX;
        b.y = cellY + CONTENT_INSET;
        b.w = cellW;
        b.h = cellH - 2 * CONTENT_INSET;

        switch (slot.type) {
            case WIDGET_CLOCK: drawClock(gfx, b, data); break;
            case WIDGET_DATE: drawDate(gfx, b, data); break;
            case WIDGET_WEATHER: drawWeather(gfx, b, data); break;
            case WIDGET_INDOOR: drawIndoor(gfx, b, data); break;
            case WIDGET_CO2: drawCo2(gfx, b, data); break;
            case WIDGET_NEXT_EVENT: drawNextEvent(gfx, b, data); break;
            case WIDGET_AGENDA: drawAgenda(gfx, b, data); break;
            case WIDGET_CLAUDE: drawClaude(gfx, b, data); break;
            default: break;
        }
    }
}
