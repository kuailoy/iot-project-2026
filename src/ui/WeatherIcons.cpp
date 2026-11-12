#include "WeatherIcons.h"
#include "Components.h"

#include <math.h>

using namespace Components;

// ---------- Code mapping ----------

WeatherIcon weatherIconFromCode(int code) {
    if (code == 511) {
        return WeatherIcon::SNOW;  // Freezing rain uses the snow icon on OpenWeatherMap
    }

    switch (code / 100) {
        case 2: return WeatherIcon::THUNDERSTORM;
        case 3: return WeatherIcon::DRIZZLE;
        case 5: return WeatherIcon::RAIN;
        case 6: return WeatherIcon::SNOW;
        case 7: return WeatherIcon::ATMOSPHERE;
    }

    if (code == 800) {
        return WeatherIcon::CLEAR;
    }
    if (code == 801) {
        return WeatherIcon::FEW_CLOUDS;
    }
    return WeatherIcon::CLOUDS;
}

const char* weatherIconName(WeatherIcon icon) {
    switch (icon) {
        case WeatherIcon::CLEAR:        return "Clear";
        case WeatherIcon::FEW_CLOUDS:   return "Few clouds";
        case WeatherIcon::CLOUDS:       return "Clouds";
        case WeatherIcon::DRIZZLE:      return "Drizzle";
        case WeatherIcon::RAIN:         return "Rain";
        case WeatherIcon::THUNDERSTORM: return "Thunderstorm";
        case WeatherIcon::SNOW:         return "Snow";
        case WeatherIcon::ATMOSPHERE:   return "Mist";
    }
    return "";
}

// ---------- Drawing helpers ----------

// Line with round ends, thickness in pixels
static void drawThickLine(Adafruit_GFX& gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t thickness) {
    int16_t steps = max(abs(x1 - x0), abs(y1 - y0));
    int16_t radius = thickness / 2;
    for (int16_t i = 0; i <= steps; i++) {
        int16_t x = x0 + (x1 - x0) * i / max(steps, (int16_t)1);
        int16_t y = y0 + (y1 - y0) * i / max(steps, (int16_t)1);
        gfx.fillCircle(x, y, radius, COLOR_FG);
    }
}

static void drawSun(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius, int16_t rayLength) {
    gfx.fillCircle(cx, cy, radius, COLOR_FG);
    gfx.fillCircle(cx, cy, radius - 3, COLOR_BG);

    int16_t inner = radius + 6;
    int16_t outer = inner + rayLength;
    for (int16_t deg = 0; deg < 360; deg += 45) {
        float rad = deg * DEG_TO_RAD;
        drawThickLine(gfx,
                      cx + lroundf(inner * cosf(rad)), cy + lroundf(inner * sinf(rad)),
                      cx + lroundf(outer * cosf(rad)), cy + lroundf(outer * sinf(rad)), 3);
    }
}

static void drawSnowflake(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius) {
    for (int16_t deg = 0; deg < 180; deg += 60) {
        float rad = (deg + 90) * DEG_TO_RAD;
        int16_t dx = lroundf(radius * cosf(rad));
        int16_t dy = lroundf(radius * sinf(rad));
        drawThickLine(gfx, cx - dx, cy - dy, cx + dx, cy + dy, 3);
    }
}

static void drawLightningBolt(Adafruit_GFX& gfx, int16_t x, int16_t y) {
    // Upper and lower half of the zigzag, white border so it stands out on the cloud
    for (int pass = 0; pass < 2; pass++) {
        uint16_t color = (pass == 0) ? COLOR_BG : COLOR_FG;
        int16_t grow = (pass == 0) ? 3 : 0;

        gfx.fillTriangle(x + 18 + grow, y - grow, x - grow, y + 31 + grow,
                         x + 21 + grow, y + 26 + grow, color);
        gfx.fillTriangle(x + 8 - grow, y + 23 - grow, x + 31 + grow, y + 21 - grow,
                         x + 5 - grow, y + 55 + grow, color);
    }
}

// ---------- Icons ----------

void drawWeatherIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, WeatherIcon icon) {
    // Icons with precipitation use a smaller cloud at the top of the box
    const int16_t smallCloudX = x + 12;
    const int16_t smallCloudW = 118;

    switch (icon) {
        case WeatherIcon::CLEAR:
            drawSun(gfx, x + 71, y + 48, 22, 12);
            break;

        case WeatherIcon::FEW_CLOUDS:
            drawSun(gfx, x + 50, y + 34, 16, 8);
            drawCloudIcon(gfx, x + 34, y + 32, 104);
            break;

        case WeatherIcon::CLOUDS:
            drawCloudIcon(gfx, x, y + 6);
            break;

        case WeatherIcon::DRIZZLE:
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            for (int16_t i = 0; i < 3; i++) {
                int16_t dropX = x + 42 + i * 28;
                drawThickLine(gfx, dropX, y + 78, dropX - 2, y + 83, 3);
                drawThickLine(gfx, dropX - 10, y + 88, dropX - 12, y + 93, 3);
            }
            break;

        case WeatherIcon::RAIN:
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            for (int16_t i = 0; i < 4; i++) {
                int16_t dropX = x + 42 + i * 22;
                drawThickLine(gfx, dropX, y + 78, dropX - 8, y + 94, 3);
            }
            break;

        case WeatherIcon::THUNDERSTORM:
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            drawLightningBolt(gfx, x + 56, y + 40);
            break;

        case WeatherIcon::SNOW:
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            drawSnowflake(gfx, x + 44, y + 85, 8);
            drawSnowflake(gfx, x + 71, y + 87, 8);
            drawSnowflake(gfx, x + 98, y + 85, 8);
            break;

        case WeatherIcon::ATMOSPHERE:
            drawThickLine(gfx, x + 22, y + 22, x + 120, y + 22, 5);
            drawThickLine(gfx, x + 6, y + 40, x + 104, y + 40, 5);
            drawThickLine(gfx, x + 34, y + 58, x + 136, y + 58, 5);
            drawThickLine(gfx, x + 14, y + 76, x + 92, y + 76, 5);
            break;
    }
}
