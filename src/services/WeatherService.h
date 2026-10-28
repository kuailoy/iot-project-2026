#pragma once

#include <Arduino.h>

// Fetches the OpenWeatherMap current-weather JSON over HTTPS.
// Certificate verification is disabled for connectivity testing.
bool fetchWeather(double latitude, double longitude, const char* api_key,
                  String& response, String& error);
