#pragma once

#include <stdint.h>

#include "AppData.h"
#include "DateUtil.h"
#include "ElectricityData.h"

// When main.cpp downloads each kind of data again. Times are millis() values.
// No Arduino code here, so the UI preview can test it on a PC.
namespace FetchSchedule {

constexpr uint32_t MINUTE_MS = 60 * 1000UL;

// One per kind of data: when the last download started
class Timer {
public:
    // True if a download should start now; then counts it as started.
    // The first call is always due. `force` (hold to refresh, new settings)
    // skips the interval, but never `minGap`.
    bool due(uint32_t nowMs, uint32_t interval, bool force, uint32_t minGap = 0) {
        if (started) {
            uint32_t since = nowMs - lastStart;  // Also right when millis() wraps around (49 days)
            if (since < minGap || (!force && since < interval)) {
                return false;
            }
        }
        started = true;
        lastStart = nowMs;
        return true;
    }

private:
    bool started = false;
    uint32_t lastStart = 0;
};

// Until the first download works, try again soon
inline uint32_t weatherInterval(const AppData& data) {
    return data.weatherValid ? 15 * MINUTE_MS : 1 * MINUTE_MS;
}

inline uint32_t calendarInterval(const AppData& data) {
    return data.calendarValid ? 30 * MINUTE_MS : 2 * MINUTE_MS;
}

// Contributions come slowly (the API allows 5000 requests an hour)
inline uint32_t githubInterval(const AppData& data) {
    return data.githubValid ? 30 * MINUTE_MS : 5 * MINUTE_MS;
}

// spot-hinta.fi allows at most one request a minute: never more often than
// this, even on "refresh"
constexpr uint32_t PRICES_MIN_GAP_MS = 2 * MINUTE_MS;

// Prices are fixed once published, so ask rarely: when today's are missing,
// every 30 min in the afternoon until tomorrow's are out, otherwise every 6 h
inline uint32_t pricesInterval(const AppData& data) {
    int32_t today = DateUtil::dayOf(data.now);
    if (!data.pricesValid || !ElectricityData::hasDay(data, today)) {
        return 5 * MINUTE_MS;
    }
    if (data.now.tm_hour >= 14 && !ElectricityData::hasDay(data, today + 1)) {
        return 30 * MINUTE_MS;  // Tomorrow's prices come out in the afternoon
    }
    return 6 * 60 * MINUTE_MS;
}

}
