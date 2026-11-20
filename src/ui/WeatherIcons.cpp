#include "WeatherIcons.h"
#include "Components.h"

#include <math.h>

using namespace Components;

// ---------- Code mapping ----------

WeatherIcon weatherIconFromCode(int code, bool night) {
    if (code == 511 || (code >= 611 && code <= 616)) {
        return WeatherIcon::SLEET;  // Freezing rain, sleet, rain and snow
    }
    if (code == 502 || code == 503 || code == 504 || code == 522) {
        return WeatherIcon::HEAVY_RAIN;
    }
    if (code == 771 || code == 781) {
        return WeatherIcon::WIND;  // Squalls, tornado
    }

    switch (code / 100) {
        case 2: return WeatherIcon::THUNDERSTORM;
        case 3: return WeatherIcon::DRIZZLE;
        case 5: return WeatherIcon::RAIN;
        case 6: return WeatherIcon::SNOW;
        case 7: return WeatherIcon::ATMOSPHERE;
    }

    switch (code) {
        case 800: return night ? WeatherIcon::CLEAR_NIGHT : WeatherIcon::CLEAR;
        case 801: return night ? WeatherIcon::FEW_CLOUDS_NIGHT : WeatherIcon::FEW_CLOUDS;
        case 803:
        case 804: return WeatherIcon::OVERCAST;
        default:  return WeatherIcon::CLOUDS;
    }
}

bool isNight(time_t now, time_t sunrise, time_t sunset) {
    long dayLength = long(sunset - sunrise);
    if (now <= 0 || sunrise <= 0 || sunset <= 0 || dayLength <= 0 || dayLength >= 86400) {
        return false;
    }
    // Time since the last sunrise, so a day-old sunrise and sunset work too
    long sinceSunrise = long((now - sunrise) % 86400);
    if (sinceSunrise < 0) {
        sinceSunrise += 86400;
    }
    return sinceSunrise >= dayLength;
}

WeatherIcon weatherIconFor(int code, time_t now, time_t sunrise, time_t sunset) {
    return weatherIconFromCode(code, isNight(now, sunrise, sunset));
}

const char* weatherIconName(WeatherIcon icon) {
    switch (icon) {
        case WeatherIcon::CLEAR:            return "Clear";
        case WeatherIcon::CLEAR_NIGHT:      return "Clear night";
        case WeatherIcon::FEW_CLOUDS:       return "Few clouds";
        case WeatherIcon::FEW_CLOUDS_NIGHT: return "Few clouds, night";
        case WeatherIcon::CLOUDS:           return "Clouds";
        case WeatherIcon::OVERCAST:         return "Overcast";
        case WeatherIcon::DRIZZLE:          return "Drizzle";
        case WeatherIcon::RAIN:             return "Rain";
        case WeatherIcon::HEAVY_RAIN:       return "Heavy rain";
        case WeatherIcon::THUNDERSTORM:     return "Thunderstorm";
        case WeatherIcon::SNOW:             return "Snow";
        case WeatherIcon::SLEET:            return "Sleet";
        case WeatherIcon::ATMOSPHERE:       return "Mist";
        case WeatherIcon::WIND:             return "Wind";
        case WeatherIcon::COUNT:            break;
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

// Crescent outline: a circle with a smaller one cut out at the top right
static void drawMoon(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius) {
    const int32_t stroke = 3;
    const int16_t cutX = cx + radius * 6 / 10;
    const int16_t cutY = cy - radius * 4 / 10;
    const int32_t cutR = radius * 8 / 10;
    const int32_t r = radius;
    for (int16_t y = cy - radius; y <= cy + radius; y++) {
        for (int16_t x = cx - radius; x <= cx + radius; x++) {
            int32_t dMoon = int32_t(x - cx) * (x - cx) + int32_t(y - cy) * (y - cy);
            int32_t dCut = int32_t(x - cutX) * (x - cutX) + int32_t(y - cutY) * (y - cutY);
            bool inside = dMoon <= r * r && dCut > cutR * cutR;
            bool deep = dMoon <= (r - stroke) * (r - stroke) && dCut > (cutR + stroke) * (cutR + stroke);
            if (inside && !deep) {
                gfx.drawPixel(x, y, COLOR_FG);
            }
        }
    }
}

// Small four-pointed star (a plus sign)
static void drawStar(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius) {
    drawThickLine(gfx, cx - radius, cy, cx + radius, cy, 3);
    drawThickLine(gfx, cx, cy - radius, cx, cy + radius, 3);
}

// Straight gust ending in a curl, up (-1) or down (+1)
static void drawGust(Adafruit_GFX& gfx, int16_t x0, int16_t x1, int16_t y, int curl) {
    const int16_t radius = 11;
    drawThickLine(gfx, x0, y, x1, y, 4);
    if (curl < 0) {
        drawThickArc(gfx, x1, y - radius, radius, -180, 90, 4);  // Bottom, right, top, left
    } else if (curl > 0) {
        drawThickArc(gfx, x1, y + radius, radius, -90, 180, 4);  // Top, right, bottom, left
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

        case WeatherIcon::CLEAR_NIGHT:
            drawMoon(gfx, x + 66, y + 50, 34);
            drawStar(gfx, x + 112, y + 22, 6);
            drawStar(gfx, x + 122, y + 64, 4);
            break;

        case WeatherIcon::FEW_CLOUDS:
            drawSun(gfx, x + 50, y + 34, 16, 8);
            drawCloudIcon(gfx, x + 34, y + 32, 104);
            break;

        case WeatherIcon::FEW_CLOUDS_NIGHT:
            drawMoon(gfx, x + 46, y + 32, 26);
            drawCloudIcon(gfx, x + 34, y + 32, 104);
            break;

        case WeatherIcon::CLOUDS:
            drawCloudIcon(gfx, x, y + 6);
            break;

        case WeatherIcon::OVERCAST:
            // A cloud behind, a bigger one in front of it
            drawCloudIcon(gfx, x + 42, y, 100);
            drawCloudIcon(gfx, x, y + 24, 118);
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

        case WeatherIcon::HEAVY_RAIN:
            // More, longer and thicker drops than RAIN
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            for (int16_t i = 0; i < 5; i++) {
                int16_t dropX = x + 36 + i * 19;
                drawThickLine(gfx, dropX, y + 76, dropX - 10, y + 95, 4);
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

        case WeatherIcon::SLEET:
            // Rain drops and snowflakes in turn
            drawCloudIcon(gfx, smallCloudX, y, smallCloudW);
            drawThickLine(gfx, x + 42, y + 78, x + 36, y + 92, 3);
            drawSnowflake(gfx, x + 64, y + 86, 7);
            drawThickLine(gfx, x + 90, y + 78, x + 84, y + 92, 3);
            drawSnowflake(gfx, x + 110, y + 86, 7);
            break;

        case WeatherIcon::ATMOSPHERE:
            drawThickLine(gfx, x + 22, y + 22, x + 120, y + 22, 5);
            drawThickLine(gfx, x + 6, y + 40, x + 104, y + 40, 5);
            drawThickLine(gfx, x + 34, y + 58, x + 136, y + 58, 5);
            drawThickLine(gfx, x + 14, y + 76, x + 92, y + 76, 5);
            break;

        case WeatherIcon::WIND:
            drawGust(gfx, x + 8, x + 92, y + 32, -1);
            drawGust(gfx, x + 18, x + 116, y + 54, 1);
            drawGust(gfx, x + 8, x + 66, y + 76, 0);
            break;

        case WeatherIcon::COUNT:
            break;
    }
}
