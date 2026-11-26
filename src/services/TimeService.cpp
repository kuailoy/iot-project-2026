#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Fetch the API's current local date and time for a city. Supply the API key
// from configuration rather than storing credentials in this source file.
bool fetchCurrentDateTime(const char* time_api_key, String& date_time,
                          const char* city = "Helsinki") {
  if (city == nullptr || time_api_key == nullptr || time_api_key[0] == '\0') {
    return false;
  }

  String url = "https://api.api-ninjas.com/v1/worldtime?city=";
  url += city;

  HTTPClient http;
  if (!http.begin(url)) {
    return false;
  }

  http.addHeader("X-Api-Key", time_api_key);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String response = http.getString();
  http.end();

  JsonDocument document;
  if (deserializeJson(document, response)) {
    return false;
  }

  const char* datetime = document["datetime"];
  if (datetime == nullptr) {
    return false;
  }

  date_time = datetime;
  return true;
}

  