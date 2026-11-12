#include "WidgetPage.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/ElectricityData.h"
#include "../../app/DateUtil.h"
#include "../../app/GitHubData.h"
#include "../../app/MusicData.h"

using namespace Components;
using namespace DateUtil;

// ---------- Grid ----------
// The whole screen is widgets: no header or footer, 4 x 3 cells (192 x 149 px).
// No frames: lines separate the widgets, like on Home.

constexpr int16_t PAD = 16;                              // Inside a widget
constexpr int16_t GRID_X = MARGIN - PAD;                 // So text starts at the page margin
constexpr int16_t GRID_TOP = 16;
constexpr int16_t GRID_BOTTOM = SCREEN_H - 16;
constexpr int16_t CELL_W = (SCREEN_W - 2 * GRID_X) / WIDGET_COLS;
constexpr int16_t CELL_H = (GRID_BOTTOM - GRID_TOP) / WIDGET_ROWS;
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

// "SW" for a wind direction in degrees (where it comes from), "" if unknown
static const char* compassPoint(int degrees) {
    static const char* const POINTS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    if (degrees < 0) {
        return "";
    }
    return POINTS[((degrees % 360) * 2 + 45) / 90 % 8];
}

// "07:42" in local time, "--" if unknown
static void formatClockTime(char* text, size_t size, time_t moment) {
    if (moment == 0) {
        snprintf(text, size, "--");
        return;
    }
    struct tm local;
    localtime_r(&moment, &local);
    strftime(text, size, "%H:%M", &local);
}

// Feels like, wind, sunrise and sunset of the large and wide weather widgets:
// value over caption (captionGap lower), in `columns` columns of colW from x,
// rows rowStep apart
static void drawWeatherDetails(Adafruit_GFX& gfx, const AppData& data, int16_t x, int16_t baseline,
                               int16_t colW, int columns, int16_t rowStep, int16_t captionGap) {
    char feels[8], wind[12], windCaption[12], sunrise[8], sunset[8];
    formatTemp(feels, sizeof(feels), data.weatherValid, data.feelsLike);
    if (data.weatherValid) {
        snprintf(wind, sizeof(wind), "%d m/s", (int)lroundf(data.windSpeed));
    } else {
        snprintf(wind, sizeof(wind), "--");
    }
    const char* from = data.weatherValid ? compassPoint(data.windDeg) : "";
    snprintf(windCaption, sizeof(windCaption), from[0] ? "Wind %s" : "Wind", from);
    formatClockTime(sunrise, sizeof(sunrise), data.weatherValid ? data.sunrise : 0);
    formatClockTime(sunset, sizeof(sunset), data.weatherValid ? data.sunset : 0);

    struct Detail {
        const char* value;
        const char* caption;
        bool degrees;
    };
    const Detail details[4] = {
        {feels, "Feels like", true}, {wind, windCaption, false}, {sunrise, "Sunrise", false}, {sunset, "Sunset", false}};
    for (int i = 0; i < 4; i++) {
        int16_t dx = x + (i % columns) * colW;
        int16_t dy = baseline + (i / columns) * rowStep;
        if (details[i].degrees) {
            drawTemperature(gfx, dx, dy, details[i].value, FONT_BODY);
        } else {
            drawText(gfx, dx, dy, details[i].value, FONT_BODY);
        }
        drawText(gfx, dx, dy + captionGap, details[i].caption, FONT_SMALL);
    }
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

    // Cloud on the left, temperature and city on the right. The large and wide
    // sizes add feels like, wind, sunrise and sunset: below, or to the right
    // (from the third column of the grid, in line with the temperature).
    // TODO: Pick the icon from the weather code (sun, rain, snow...)
    bool wide = b.w >= 600;
    const int16_t detailsX = b.x + 2 * CELL_W;
    drawCloudIcon(gfx, b.x + PAD, b.y + 44);
    int16_t textX = b.x + PAD + 160;
    drawTemperature(gfx, textX, b.y + 90, text, FONT_TEMP);

    char city[sizeof(data.city)];
    strlcpy(city, data.city, sizeof(city));
    fitText(gfx, city, (wide ? detailsX - PAD : b.right() - PAD) - textX, FONT_SMALL);
    drawText(gfx, textX, b.y + 120, city, FONT_SMALL);

    if (wide) {
        // Captions in line with the city
        drawWeatherDetails(gfx, data, detailsX, b.y + 90, (b.right() - PAD - detailsX) / 4, 4, 0, 30);
    } else if (b.h >= 200) {
        drawDottedLine(gfx, b.x + PAD, b.right() - PAD, b.y + 152);
        drawWeatherDetails(gfx, data, b.x + PAD, b.y + 188, (b.w - 2 * PAD) / 2, 2, 66, 22);
    }
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

// ---------- Electricity spot price ----------

static void drawPrice(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    using namespace ElectricityData;
    char text[40];
    drawLabel(gfx, b, "ELECTRICITY");

    DayPrices today = data.timeValid ? dayPrices(data, DateUtil::dayOf(data.now)) : DayPrices();
    const PricePoint* now = priceNow(data);
    if (!today.any || now == nullptr) {
        drawText(gfx, b.x + PAD, b.y + 80, "--", FONT_TEMP);
        drawText(gfx, b.x + PAD, b.y + 110, data.pricesValid ? "No price now" : "Loading prices", FONT_SMALL);
        return;
    }

    int hour = data.now.tm_hour;
    int cheapest = cheapestHours(today, hour, 3);
    char price[12];
    formatCents(price, sizeof(price), now->cents);

    // Price now, with the unit right after it
    auto drawNow = [&](int16_t baseline) {
        drawText(gfx, b.x + PAD, baseline, price, FONT_TEMP);
        drawText(gfx, gfx.getCursorX() + 8, baseline, "c/kWh", FONT_SMALL);
    };

    if (b.w < 300) {
        // Small: price and how it compares with today
        drawNow(b.y + 82);
        drawText(gfx, b.x + PAD, b.y + 114, levelText(now->cents, today.average), FONT_SMALL_BOLD);
        return;
    }

    char window[16] = "";
    if (cheapest >= 0) {
        formatHours(window, sizeof(window), cheapest, 3);
    }

    if (b.h >= 200) {
        // Large: price, level, chart with hours, cheapest window
        formatCents(text, sizeof(text), today.average);
        char avg[24];
        snprintf(avg, sizeof(avg), "Avg %s", text);
        drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, avg, FONT_SMALL);
        drawNow(b.y + 80);
        drawText(gfx, b.x + PAD, b.y + 108, levelText(now->cents, today.average), FONT_SMALL_BOLD);
        drawPriceChart(gfx, b.x + PAD, b.y + 126, b.w - 2 * PAD, 100, today.hourly, today.hasHour, hour, cheapest,
                       cheapest >= 0 ? 3 : 0, true);
        if (cheapest >= 0) {
            snprintf(text, sizeof(text), "Cheapest %s", window);
            drawText(gfx, b.x + PAD, b.y + 274, text, FONT_SMALL);
        }
        return;
    }

    if (b.w >= 600) {
        // Wide: price on the left, today's chart on the right
        if (cheapest >= 0) {
            snprintf(text, sizeof(text), "Cheapest %s", window);
            drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, text, FONT_SMALL);
        }
        drawNow(b.y + 82);
        drawText(gfx, b.x + PAD, b.y + 112, levelText(now->cents, today.average), FONT_SMALL_BOLD);
        int16_t chartX = b.x + PAD + 250;
        drawPriceChart(gfx, chartX, b.y + 44, b.right() - PAD - chartX, 66, today.hourly, today.hasHour, hour,
                       cheapest, cheapest >= 0 ? 3 : 0, true);
        return;
    }

    // Medium: price on the left, small chart on the right
    drawNow(b.y + 82);
    drawText(gfx, b.x + PAD, b.y + 112, levelText(now->cents, today.average), FONT_SMALL_BOLD);
    int16_t chartX = b.x + PAD + 172;
    drawPriceChart(gfx, chartX, b.y + 40, b.right() - PAD - chartX, 74, today.hourly, today.hasHour, hour, cheapest,
                   cheapest >= 0 ? 3 : 0, false);
}

// ---------- Page ----------

// ---------- GitHub contribution graph ----------

// "1,234"
static void formatCount(char* text, size_t size, uint32_t value) {
    if (value >= 1000) {
        snprintf(text, size, "%lu,%03lu", (unsigned long)(value / 1000), (unsigned long)(value % 1000));
    } else {
        snprintf(text, size, "%lu", (unsigned long)value);
    }
}

// One day. E-Ink has no greens and fine gray patterns blur together, so
// GitHub's four levels become bigger black squares (the busiest fills the
// cell); a day without contributions is a small dot, which keeps the grid.
static void drawContributionCell(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t size, int level) {
    int16_t side = level == 0 ? 2 : size - 2 * (4 - level);
    int16_t inset = (size - side) / 2;
    gfx.fillRect(x + inset, y + inset, side, side, COLOR_FG);
}

// The graph like on a GitHub profile: a column per week, Sunday at the top,
// the newest week on the right. As many weeks as fit in w, centered.
static void drawContributionGraph(Adafruit_GFX& gfx, const GitHubContributions& g, int16_t x, int16_t y,
                                  int16_t w, int16_t pitch) {
    const int16_t gap = 2;
    int32_t firstDay = g.lastDay - g.dayCount + 1;
    int32_t lastWeek = g.lastDay - GitHubData::rowOf(g.lastDay);  // Its Sunday
    int32_t firstWeek = firstDay - GitHubData::rowOf(firstDay);
    int weeksInData = int((lastWeek - firstWeek) / 7) + 1;
    int fit = (w + gap) / pitch;
    int weeks = weeksInData < fit ? weeksInData : fit;
    int16_t left = x + (w - (weeks * pitch - gap)) / 2;

    for (int column = 0; column < weeks; column++) {
        int32_t sunday = lastWeek - 7 * (weeks - 1 - column);
        for (int row = 0; row < 7; row++) {
            int level = GitHubData::levelOn(g, sunday + row);
            if (level >= 0) {  // Not before the data, not after today
                drawContributionCell(gfx, left + column * pitch, y + row * pitch, pitch - gap, level);
            }
        }
    }
}

static void drawGitHub(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[80], total[16], today[16];
    drawLabel(gfx, b, "GITHUB");
    bool small = b.w < 300;

    // A refused token shows even over an older graph: it won't update any more
    if (!data.githubTokenSet || !data.githubValid || data.githubTokenRefused) {
        bool fixOnPhone = !data.githubTokenSet || data.githubTokenRefused;
        const char* line = !data.githubTokenSet     ? "Add a GitHub token"
                         : data.githubTokenRefused  ? "Token not accepted"
                         : data.wifiConnected       ? "Loading..."
                                                    : "No Wi-Fi";
        drawText(gfx, b.x + PAD, b.y + 72, line, FONT_SMALL_BOLD);
        if (fixOnPhone) {
            drawText(gfx, b.x + PAD, b.y + 98, small ? "on desk.local" : "on desk.local > Settings", FONT_SMALL);
        }
        return;
    }

    const GitHubContributions& g = data.github;
    int streak = GitHubData::streak(g);
    formatCount(total, sizeof(total), g.total);
    snprintf(today, sizeof(today), "%d today", GitHubData::today(data));
    bool large = b.h >= 200;
    int16_t graphW = b.w - 2 * PAD;

    // Next to the label: today, and in the wider sizes the year and the streak
    if (b.w >= 600) {
        char login[44] = "";
        if (g.login[0]) {
            snprintf(login, sizeof(login), "@%s, ", g.login);
        }
        snprintf(text, sizeof(text), "%s%s, %s in the last year, %d-day streak", login, today, total, streak);
    } else if (!small && !large) {
        snprintf(text, sizeof(text), "%s, %s in the last year", today, total);
    } else {
        strlcpy(text, today, sizeof(text));
    }
    fitText(gfx, text, graphW - measureText(gfx, "GITHUB", FONT_SMALL) - 16, FONT_SMALL);
    drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, text, FONT_SMALL);

    if (!large) {
        drawContributionGraph(gfx, g, b.x + PAD, b.y + 44, graphW, 13);
        return;
    }

    // Large: bigger squares, then the year and the streak as big numbers
    drawContributionGraph(gfx, g, b.x + PAD, b.y + 44, graphW, 16);
    snprintf(text, sizeof(text), "%d", streak);
    drawValue(gfx, b.x + PAD, b.y + 218, total, "in the last year", false);
    drawValue(gfx, b.x + PAD + graphW / 2, b.y + 218, text, "day streak", false);
}

// ---------- Now playing ----------

// Text cut to fit maxW, in the font given
static void drawFitted(Adafruit_GFX& gfx, int16_t x, int16_t y, const char* text, int16_t maxW,
                       const GFXfont* font) {
    char line[96];
    strlcpy(line, text, sizeof(line));
    fitText(gfx, line, maxW, font);
    drawText(gfx, x, y, line, font);
}

// "1:42 / 3:38", or "1:42" without a length (live)
static void formatProgress(char* text, size_t size, const AppData& data) {
    char now[12], length[12];
    MusicData::formatTime(now, sizeof(now), MusicData::position(data));
    if (data.musicDuration == 0) {
        strlcpy(text, now, size);
        return;
    }
    MusicData::formatTime(length, sizeof(length), data.musicDuration);
    snprintf(text, size, "%s / %s", now, length);
}

// Outlined bar, filled up to the time into the song
static void drawProgressBar(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h, const AppData& data) {
    if (data.musicDuration == 0) {
        return;
    }
    gfx.drawRect(x, y, w, h, COLOR_FG);
    gfx.fillRect(x, y, int32_t(w) * MusicData::position(data) / data.musicDuration, h, COLOR_FG);
}

static void drawMusic(Adafruit_GFX& gfx, const Box& b, const AppData& data) {
    char text[96];
    const int16_t innerW = b.w - 2 * PAD;
    bool small = b.w < 300;

    if (!MusicData::active(data)) {
        drawLabel(gfx, b, "MUSIC");
        drawText(gfx, b.x + PAD, b.y + 72, "Nothing playing", FONT_SMALL_BOLD);
        if (MusicData::hasLast(data)) {
            snprintf(text, sizeof(text), small ? "%s" : "Last: %s", data.musicTitle);
            drawFitted(gfx, b.x + PAD, b.y + 98, text, innerW, FONT_SMALL);
        } else if (!data.musicValid && !small) {
            drawText(gfx, b.x + PAD, b.y + 98, "Add the browser extension", FONT_SMALL);
        }
        if (b.h >= 200) {
            drawMusicIcon(gfx, b.cx() - 60, b.y + 132, 120, false);
        }
        return;
    }

    drawLabel(gfx, b, data.musicPlaying ? "NOW PLAYING" : "PAUSED");
    formatProgress(text, sizeof(text), data);

    if (small) {
        drawFitted(gfx, b.x + PAD, b.y + 68, data.musicTitle, innerW, FONT_SMALL_BOLD);
        drawFitted(gfx, b.x + PAD, b.y + 92, data.musicArtist, innerW, FONT_SMALL);
        drawProgressBar(gfx, b.x + PAD, b.y + 104, innerW, 7, data);
        drawText(gfx, b.x + PAD, b.y + 130, text, FONT_SMALL);
        return;
    }

    if (b.h >= 200) {
        // Large: note and source on top, then the song over the full width
        drawMusicIcon(gfx, b.x + PAD, b.y + 44, 104, true);
        drawText(gfx, b.x + PAD + 124, b.y + 88, "YouTube Music", FONT_SMALL_BOLD);
        drawText(gfx, b.x + PAD + 124, b.y + 114, data.musicPlaying ? "Playing" : "Paused", FONT_BODY);
        bool big = measureText(gfx, data.musicTitle, FONT_TEMP) <= innerW;
        drawFitted(gfx, b.x + PAD, b.y + 184, data.musicTitle, innerW, big ? FONT_TEMP : FONT_BODY);
        drawFitted(gfx, b.x + PAD, b.y + 212, data.musicArtist, innerW, FONT_BODY);
        drawProgressBar(gfx, b.x + PAD, b.y + 228, innerW, 10, data);
        drawText(gfx, b.x + PAD, b.y + 262, text, FONT_SMALL);
        return;
    }

    // Medium and wide: note on the left, the song next to it
    bool wide = b.w >= 600;
    int16_t iconSize = wide ? 92 : 64;
    drawMusicIcon(gfx, b.x + PAD, b.y + 40, iconSize, true);
    int16_t x = b.x + PAD + iconSize + 16;
    int16_t w = b.right() - PAD - x;
    if (wide) {
        // Times next to the label, so the bar can sit under the song
        drawTextRight(gfx, b.right() - PAD, b.y + LABEL_Y, text, FONT_SMALL);
        bool big = measureText(gfx, data.musicTitle, FONT_TEMP) <= w;
        drawFitted(gfx, x, b.y + 80, data.musicTitle, w, big ? FONT_TEMP : FONT_BODY);
        char byline[96];
        snprintf(byline, sizeof(byline), data.musicAlbum[0] ? "%s - %s" : "%s", data.musicArtist, data.musicAlbum);
        drawFitted(gfx, x, b.y + 106, byline, w, FONT_BODY);
        drawProgressBar(gfx, x, b.y + 118, w, 8, data);
    } else {
        drawFitted(gfx, x, b.y + 66, data.musicTitle, w, FONT_BODY);
        drawFitted(gfx, x, b.y + 90, data.musicArtist, w, FONT_SMALL);
        drawProgressBar(gfx, x, b.y + 102, w, 7, data);
        drawText(gfx, x, b.y + 128, text, FONT_SMALL);
    }
}

void WidgetPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    // No header and footer: the screen is for the widgets (the controls are
    // the same as on Home: press = menu, hold = refresh, touch = Home)
    if (data.widgetCount == 0) {
        int16_t middle = SCREEN_H / 2;
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
            case WIDGET_PRICE: drawPrice(gfx, b, data); break;
            case WIDGET_GITHUB: drawGitHub(gfx, b, data); break;
            case WIDGET_MUSIC: drawMusic(gfx, b, data); break;
            default: break;
        }
    }
}
