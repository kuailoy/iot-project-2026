#include "ClaudeUsage.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../app/DateUtil.h"

using DateUtil::parseIsoTime;

bool parseClaudeUsage(const char* json, ClaudeUsage& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc["session"].is<float>() || !doc["weekly"].is<float>()) {
        return false;
    }
    out.sessionPercent = doc["session"].as<float>();
    out.weeklyPercent = doc["weekly"].as<float>();
    out.sessionResets = parseIsoTime(doc["session_resets"] | "");
    out.weeklyResets = parseIsoTime(doc["weekly_resets"] | "");
    return true;
}
