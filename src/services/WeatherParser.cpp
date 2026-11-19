#include "WeatherParser.h"

#include <math.h>

bool readWeather(JsonObjectConst answer, Weather& out) {
    float temperature = answer["main"]["temp"] | NAN;
    JsonVariantConst code = answer["weather"][0]["id"];
    if (!isfinite(temperature) || !code.is<int>()) {
        return false;
    }
    out.temperature = temperature;
    out.code = code.as<int>();

    // For the big weather widgets. Optional: calm wind has no direction, and
    // there is no sunrise or sunset during polar day or night.
    out.feelsLike = answer["main"]["feels_like"] | temperature;
    out.windSpeed = answer["wind"]["speed"] | 0.0f;
    out.windDeg = answer["wind"]["deg"] | -1;
    out.sunrise = answer["sys"]["sunrise"] | 0L;
    out.sunset = answer["sys"]["sunset"] | 0L;
    return true;
}

bool storeWeather(const Weather& weather, AppData& data) {
    bool changed = !data.weatherValid ||
                   lroundf(weather.temperature) != lroundf(data.outdoorTemp) ||
                   weather.code != data.weatherCode ||
                   lroundf(weather.feelsLike) != lroundf(data.feelsLike) ||
                   lroundf(weather.windSpeed) != lroundf(data.windSpeed) || weather.windDeg != data.windDeg ||
                   weather.sunrise / 60 != data.sunrise / 60 || weather.sunset / 60 != data.sunset / 60;

    data.outdoorTemp = weather.temperature;
    data.weatherCode = weather.code;
    data.feelsLike = weather.feelsLike;
    data.windSpeed = weather.windSpeed;
    data.windDeg = weather.windDeg;
    data.sunrise = weather.sunrise;
    data.sunset = weather.sunset;
    data.weatherValid = true;
    return changed;
}
