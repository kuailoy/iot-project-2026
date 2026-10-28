#include "HomePage.h"
#include "../Components.h"
#include "../WeatherIcons.h"

#include <math.h>
#include <stdio.h>

using namespace Components;

// Rounded whole degrees, or "--" when there is no data yet
static void formatTemp(char* buffer, size_t size, bool valid, float value) {
    if (valid) {
        snprintf(buffer, size, "%d", (int)lroundf(value));
    } else {
        snprintf(buffer, size, "--");
    }
}

void HomePage::draw(Adafruit_GFX& gfx, const AppData& data) {
    char text[48];

    drawHeader(gfx, data);

    // ---------- Outdoor weather ----------
    drawWeatherIcon(gfx, 49, 141, weatherIconFromCode(data.weatherCode));
    formatTemp(text, sizeof(text), data.weatherValid, data.outdoorTemp);
    drawTemperature(gfx, 212, 194, text, FONT_TEMP);
    drawText(gfx, 212, 232, data.city, FONT_BODY);

    gfx.fillRect(481, 121, 2, 140, EPD_BLACK);

    // ---------- Indoor sensor ----------
    drawText(gfx, 516, 146, "INDOOR", FONT_SMALL);
    formatTemp(text, sizeof(text), data.sensorValid, data.indoorTemp);
    drawTemperature(gfx, 516, 198, text, FONT_TEMP);
    if (data.sensorValid) {
        snprintf(text, sizeof(text), "%d%% RH", (int)lroundf(data.humidity));
    } else {
        snprintf(text, sizeof(text), "--%% RH");
    }
    drawText(gfx, 516, 240, text, FONT_BODY);

    gfx.fillRect(MARGIN, 282, SCREEN_W - 2 * MARGIN, 2, EPD_BLACK);

    // ---------- Next calendar event ----------
    drawCalendarIcon(gfx, 39, 313);
    drawText(gfx, 108, 330, "NEXT EVENT", FONT_SMALL);
    if (data.eventValid) {
        snprintf(text, sizeof(text), "%s %s", data.eventTime, data.eventTitle);
    } else {
        snprintf(text, sizeof(text), "No upcoming events");
    }
    drawText(gfx, 108, 362, text, FONT_BODY);

    drawFooter(gfx, "Turn: mode      Press: refresh");
}
