#pragma once

#include <Arduino.h>

// Fetches the OpenWeatherMap current-weather JSON over verified HTTPS.
// Supply the API key and the PEM-encoded root CA used to verify the server.
bool fetchWeather(double latitude, double longitude, const char* api_key,
                  const char* root_ca, String& response, String& error);
