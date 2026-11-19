#pragma once

#include <ArduinoJson.h>
#include <time.h>

#include "../app/AppData.h"

// Current weather from a parsed OpenWeatherMap answer (see WeatherService.cpp).
// No Arduino or network code, so the UI preview can test it on a PC.
struct Weather {
    float temperature = 0;  // C
    int code = 0;           // OpenWeatherMap condition code
    float feelsLike = 0;    // C, the temperature if the answer has none
    float windSpeed = 0;    // m/s
    int windDeg = -1;       // Where the wind comes from; -1 = calm or unknown
    time_t sunrise = 0;     // UTC; 0 = none (polar day or night)
    time_t sunset = 0;
};

// False if the answer has no temperature or condition code (an error answer)
bool readWeather(JsonObjectConst answer, Weather& out);

// Puts the weather into data. True if a value the screen shows changed
// (whole degrees, whole m/s, minutes of sunrise and sunset): then redraw.
bool storeWeather(const Weather& weather, AppData& data);
