#include "WeatherService.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// "Äänekoski" -> "%C3%84%C3%A4nekoski"
static String urlEncode(const String& text) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < text.length(); i++) {
    uint8_t c = uint8_t(text[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += char(c);
    } else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 15];
    }
  }
  return out;
}

bool findCities(const String& query, const char* api_key, String& json, String& error) {
  if (api_key == nullptr || api_key[0] == '\0') {
    error = "OpenWeatherMap API key is missing";
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();  // TODO: Verify the server certificate (as for the weather)

  String url = "https://api.openweathermap.org/geo/1.0/direct?limit=5&q=" + urlEncode(query) +
               "&appid=" + api_key;
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    error = "Could not start the city search";
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    error = status > 0 ? String("HTTP ") + status : http.errorToString(status);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, http.getStream());
  http.end();
  if (parseError) {
    error = parseError.c_str();
    return false;
  }

  // Only what the page needs: name, "state, country", coordinates
  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObject place : doc.as<JsonArray>()) {
    JsonObject item = list.add<JsonObject>();
    item["n"] = place["name"] | "";
    String region = place["state"] | "";
    const char* country = place["country"] | "";
    if (region.length() > 0 && country[0] != '\0') {
      region += ", ";
    }
    region += country;
    item["r"] = region;
    item["lat"] = place["lat"] | 0.0;
    item["lon"] = place["lon"] | 0.0;
  }
  json = "";
  serializeJson(out, json);
  return true;
}

bool fetchWeather(double latitude, double longitude, const char* api_key,
                  String& response, String& error) {
  if (api_key == nullptr || api_key[0] == '\0') {
    error = "OpenWeatherMap API key is missing";
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(latitude, 6);
  url += "&lon=";
  url += String(longitude, 6);
  url += "&appid=";
  url += api_key;
  url += "&units=metric";

  HTTPClient http;
  http.setTimeout(15000);
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
