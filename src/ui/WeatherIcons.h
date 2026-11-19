#pragma once

#include <Adafruit_GFX.h>
#include <time.h>

// Weather icons for OpenWeatherMap condition codes (weather[0].id)
// Codes: https://openweathermap.org/api/weather-conditions

enum class WeatherIcon {
    CLEAR,             // 800
    CLEAR_NIGHT,       // 800 between sunset and sunrise: moon and stars
    FEW_CLOUDS,        // 801, sun behind a cloud
    FEW_CLOUDS_NIGHT,  // 801 at night, moon behind a cloud
    CLOUDS,            // 802, also used when the code is unknown
    OVERCAST,          // 803-804, two clouds
    DRIZZLE,           // 3xx
    RAIN,              // 500, 501, 520, 521, 531
    HEAVY_RAIN,        // 502-504, 522
    THUNDERSTORM,      // 2xx
    SNOW,              // 600-602, 620-622
    SLEET,             // 511 (freezing rain), 611-616 (sleet, rain and snow)
    ATMOSPHERE,        // 7xx: mist, fog, haze, dust...
    WIND,              // 771 squalls, 781 tornado
    COUNT
};

// `night`: use the moon instead of the sun (see isNight)
WeatherIcon weatherIconFromCode(int code, bool night = false);

// Between sunset and sunrise. sunrise and sunset are one day's (UTC), as
// OpenWeatherMap sends them; they still work a day later. 0 = unknown
// (polar day or night): day.
bool isNight(time_t now, time_t sunrise, time_t sunset);

// The icon for the weather now, with the night icons after sunset
WeatherIcon weatherIconFor(int code, time_t now, time_t sunrise, time_t sunset);

// Short English label, e.g. "Rain"
const char* weatherIconName(WeatherIcon icon);

// Draws the icon inside a 142x96 box, x, y = top-left corner
void drawWeatherIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, WeatherIcon icon);
