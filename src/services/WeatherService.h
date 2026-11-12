#pragma once

#include <Arduino.h>

// Fetches the OpenWeatherMap current-weather JSON over HTTPS.
// Certificate verification is disabled for connectivity testing.
bool fetchWeather(double latitude, double longitude, const char* api_key,
                  String& response, String& error);

// Looks up places by name (OpenWeatherMap geocoding, up to 5 matches) for the
// city choice on the settings page. `json` gets a short list for the page:
// [{"n":"Kuopio","r":"North Savo, FI","lat":62.89,"lon":27.68}, ...]
bool findCities(const String& query, const char* api_key, String& json, String& error);
