#pragma once

#include "AppData.h"
#include "DateUtil.h"

// Read-only helpers for the GitHub contribution graph in AppData (GitHub widget).
namespace GitHubData {

// Level of a day, 0 (none) to 4 (most); -1 if the day is not in the data
inline int levelOn(const GitHubContributions& g, int32_t day) {
    int32_t index = int32_t(g.dayCount) - 1 - (g.lastDay - day);
    if (index < 0 || index >= g.dayCount) {
        return -1;
    }
    return g.levels[index];
}

// Row of a day in the graph: 0 = Sunday at the top, like on GitHub
inline int rowOf(int32_t day) {
    return (DateUtil::weekday(day) + 1) % 7;
}

// Days in a row with contributions up to the newest day. A newest day
// without any doesn't end the streak: it isn't over yet.
inline int streak(const GitHubContributions& g) {
    int i = int(g.dayCount) - 1;
    if (i >= 0 && g.levels[i] == 0) {
        i--;
    }
    int days = 0;
    while (i >= 0 && g.levels[i] > 0) {
        days++;
        i--;
    }
    return days;
}

// Contributions today: the newest day, if that is today (0 otherwise)
inline int today(const AppData& data) {
    if (!data.timeValid || data.github.lastDay != DateUtil::dayOf(data.now)) {
        return 0;
    }
    return data.github.lastDayCount;
}

}
