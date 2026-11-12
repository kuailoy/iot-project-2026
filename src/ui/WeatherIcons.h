#pragma once

#include <Adafruit_GFX.h>

// Weather icons for OpenWeatherMap condition codes (weather[0].id)
// Codes: https://openweathermap.org/api/weather-conditions

enum class WeatherIcon {
    CLEAR,         // 800
    FEW_CLOUDS,    // 801, sun behind a cloud
    CLOUDS,        // 802-804, also used when the code is unknown
    DRIZZLE,       // 3xx
    RAIN,          // 5xx except 511
    THUNDERSTORM,  // 2xx
    SNOW,          // 6xx and 511 (freezing rain)
    ATMOSPHERE     // 7xx: mist, fog, haze, dust...
};

WeatherIcon weatherIconFromCode(int code);

// Short English label, e.g. "Rain"
const char* weatherIconName(WeatherIcon icon);

// Draws the icon inside a 142x96 box, x, y = top-left corner
void drawWeatherIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, WeatherIcon icon);
