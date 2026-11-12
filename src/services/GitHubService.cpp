#include "GitHubService.h"
#include "GitHubParser.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

const char* const GRAPHQL_URL = "https://api.github.com/graphql";

// The last year (the API's default range), the same days as the graph on a profile
const char* const QUERY =
    "{\"query\":\"query{viewer{login contributionsCollection{contributionCalendar{totalContributions "
    "weeks{contributionDays{contributionCount contributionLevel date}}}}}}\"}";

}  // namespace

bool fetchContributions(const char* token, GitHubContributions& out, String& error, bool& tokenRefused) {
    tokenRefused = false;
    if (token == nullptr || token[0] == '\0') {
        error = "no GitHub token: add it on the settings page (desk.local)";
        return false;
    }

    // TODO: Verify the server certificate (the weather request skips it too)
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);
    http.useHTTP10(true);  // No chunked transfer: the JSON can be read straight from the stream
    if (!http.begin(client, GRAPHQL_URL)) {
        error = "Could not start the GitHub request";
        return false;
    }
    http.addHeader("Authorization", String("bearer ") + token);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "desk-display");  // The API refuses requests without one

    int status = http.POST(QUERY);
    tokenRefused = status == 401;  // Wrong, expired or revoked
    if (status != HTTP_CODE_OK) {
        error = status == 401 ? String("the token was refused (HTTP 401): check it on desk.local")
              : status > 0    ? String("HTTP ") + status
                              : HTTPClient::errorToString(status);
        http.end();
        return false;
    }

    // About 25 KB for a year; keep only what the graph needs
    JsonDocument filter;
    JsonObject viewer = filter["data"]["viewer"].to<JsonObject>();
    viewer["login"] = true;
    JsonObject calendar = viewer["contributionsCollection"]["contributionCalendar"].to<JsonObject>();
    calendar["totalContributions"] = true;
    JsonObject day = calendar["weeks"][0]["contributionDays"][0].to<JsonObject>();
    day["contributionCount"] = true;
    day["contributionLevel"] = true;
    day["date"] = true;
    filter["errors"][0]["message"] = true;

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (parseError) {
        error = String("GitHub JSON: ") + parseError.c_str();
        return false;
    }

    // GraphQL reports problems inside an HTTP 200 answer
    const char* message = doc["errors"][0]["message"];
    if (message != nullptr) {
        error = String("GitHub: ") + message;
        return false;
    }
    if (!readContributions(doc.as<JsonObjectConst>(), out)) {
        error = "no contribution calendar in the answer";
        return false;
    }
    return true;
}
