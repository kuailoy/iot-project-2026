#include "ElectricityService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../app/DateUtil.h"

// Today and tomorrow (when published), Finland, 15-minute prices
static const char* const PRICES_URL =
    "https://api.spot-hinta.fi/TodayAndDayForward?region=FI&priceResolution=15";

int fetchPrices(PricePoint* out, int capacity, String& error) {
    // TODO: Verify the server certificate (the weather request skips it too)
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);
    http.useHTTP10(true);  // No chunked transfer: the JSON can be read straight from the stream
    if (!http.begin(client, PRICES_URL)) {
        error = "Could not start the price request";
        return -1;
    }
    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        error = status == 429 ? String("too many requests (HTTP 429)")
              : status > 0    ? String("HTTP ") + status
                              : HTTPClient::errorToString(status);
        http.end();
        return -1;
    }

    // [{"Rank": 27, "DateTime": "2026-11-10T00:00:00+02:00", "PriceNoTax": 0.01578, "PriceWithTax": 0.0198}, ...]
    // Keep only the time and the price with VAT (EUR/kWh)
    JsonDocument filter;
    filter[0]["DateTime"] = true;
    filter[0]["PriceWithTax"] = true;
    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (parseError) {
        error = String("price JSON: ") + parseError.c_str();
        return -1;
    }

    int count = readPrices(doc.as<JsonArrayConst>(), out, capacity);
    if (count == 0) {
        error = "no prices in the answer";
        return -1;
    }
    return count;
}

int readPrices(JsonArrayConst list, PricePoint* out, int capacity) {
    int count = 0;
    for (JsonObjectConst item : list) {
        if (count >= capacity) {
            break;
        }
        time_t start = DateUtil::parseIsoTime(item["DateTime"] | "");
        if (start == 0 || !item["PriceWithTax"].is<float>()) {
            continue;
        }
        out[count].start = start;
        out[count].cents = item["PriceWithTax"].as<float>() * 100.0f;
        count++;
    }

    // Sorted by time (the API already is, this keeps it safe)
    for (int i = 1; i < count; i++) {
        for (int j = i; j > 0 && out[j].start < out[j - 1].start; j--) {
            PricePoint tmp = out[j];
            out[j] = out[j - 1];
            out[j - 1] = tmp;
        }
    }
    return count;
}
