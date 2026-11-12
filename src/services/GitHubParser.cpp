#include "GitHubParser.h"

#include <stdio.h>
#include <string.h>

#include "../app/DateUtil.h"

namespace {

// "2026-11-12" -> day number, 0 if invalid
int32_t parseDate(const char* text) {
    int year, month, day;
    if (text == nullptr || sscanf(text, "%4d-%2d-%2d", &year, &month, &day) != 3) {
        return 0;
    }
    return DateUtil::daysFromCivil(year, month, day);
}

// GitHub's ContributionLevel -> 0..4
uint8_t parseLevel(const char* level) {
    static const char* const NAMES[5] = {"NONE", "FIRST_QUARTILE", "SECOND_QUARTILE", "THIRD_QUARTILE",
                                         "FOURTH_QUARTILE"};
    for (uint8_t i = 0; i < 5; i++) {
        if (level != nullptr && strcmp(level, NAMES[i]) == 0) {
            return i;
        }
    }
    return 0;
}

}  // namespace

bool readContributions(JsonObjectConst answer, GitHubContributions& out) {
    JsonObjectConst viewer = answer["data"]["viewer"];
    JsonObjectConst calendar = viewer["contributionsCollection"]["contributionCalendar"];
    JsonArrayConst weeks = calendar["weeks"];
    if (weeks.isNull()) {
        return false;
    }

    // The newest day first: every day is placed by its date from there
    int32_t lastDay = 0;
    int32_t firstDay = 0;
    for (JsonObjectConst week : weeks) {
        for (JsonObjectConst d : week["contributionDays"].as<JsonArrayConst>()) {
            int32_t date = parseDate(d["date"]);
            if (date == 0) {
                continue;
            }
            if (lastDay == 0 || date > lastDay) {
                lastDay = date;
            }
            if (firstDay == 0 || date < firstDay) {
                firstDay = date;
            }
        }
    }
    if (lastDay == 0) {
        return false;
    }

    out = GitHubContributions();
    strlcpy(out.login, viewer["login"] | "", sizeof(out.login));
    out.total = calendar["totalContributions"] | 0;
    out.lastDay = lastDay;
    int32_t days = lastDay - firstDay + 1;
    out.dayCount = uint16_t(days < GITHUB_MAX_DAYS ? days : GITHUB_MAX_DAYS);  // The newest ones

    for (JsonObjectConst week : weeks) {
        for (JsonObjectConst d : week["contributionDays"].as<JsonArrayConst>()) {
            int32_t date = parseDate(d["date"]);
            int32_t index = int32_t(out.dayCount) - 1 - (lastDay - date);
            if (date == 0 || index < 0) {
                continue;
            }
            out.levels[index] = parseLevel(d["contributionLevel"]);
            if (date == lastDay) {
                out.lastDayCount = d["contributionCount"] | 0;
            }
        }
    }
    return true;
}
