#include "ClaudeUsage.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../app/DateUtil.h"

static bool readDigits(const char* text, int count, int& out) {
    out = 0;
    for (int i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        out = out * 10 + (text[i] - '0');
    }
    return true;
}

time_t parseIsoTime(const char* text) {
    if (text == nullptr || strlen(text) < 19) {
        return 0;
    }
    int year, month, day, hour, minute, second;
    if (!readDigits(text, 4, year) || text[4] != '-' || !readDigits(text + 5, 2, month) || text[7] != '-' ||
        !readDigits(text + 8, 2, day) || text[10] != 'T' || !readDigits(text + 11, 2, hour) ||
        !readDigits(text + 14, 2, minute) || !readDigits(text + 17, 2, second)) {
        return 0;
    }

    // Skip fractions of a second, then read the offset: Z, +HH:MM or -HH:MM
    const char* p = text + 19;
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            p++;
        }
    }
    long offset = 0;
    if (*p == '+' || *p == '-') {
        int offsetHours, offsetMinutes;
        if (!readDigits(p + 1, 2, offsetHours) || p[3] != ':' || !readDigits(p + 4, 2, offsetMinutes)) {
            return 0;
        }
        offset = (offsetHours * 60L + offsetMinutes) * 60L * (*p == '-' ? -1 : 1);
    }

    long long days = DateUtil::daysFromCivil(year, month, day);
    return time_t(days * 86400LL + hour * 3600LL + minute * 60LL + second - offset);
}

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
