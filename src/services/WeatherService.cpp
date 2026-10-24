#include "WeatherService.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool fetchWeather(double latitude, double longitude, const char* api_key,
                  const char* root_ca, String& response, String& error) {
  if (api_key == nullptr || api_key[0] == '\0') {
    error = "OpenWeatherMap API key is missing";
    return false;
  }
  if (root_ca == nullptr || root_ca[0] == '\0') {
    error = "HTTPS root CA certificate is missing";
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(root_ca);

  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(latitude, 6);
  url += "&lon=";
  url += String(longitude, 6);
  url += "&appid=";
  url += api_key;

  HTTPClient http;
  if (!http.begin(client, url)) {
    error = "Could not initialize the weather request";
    return false;
  }

  http.setTimeout(15000);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    error = "Weather request failed: ";
    error += status > 0 ? String("HTTP ") + status : http.errorToString(status);
    http.end();
    return false;
  }

  response = http.getString();
  http.end();
  error = "";
  return true;
}
