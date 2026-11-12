#pragma once

#include <math.h>
#include <stdio.h>

#include "AppData.h"
#include "DateUtil.h"

// Read-only helpers for the electricity spot prices in AppData, used by the
// Electricity page, its widget and the menu. Prices are per 15 minutes in
// c/kWh including VAT; the chart uses hourly averages.
namespace ElectricityData {

constexpr time_t SLOT_SECONDS = 15 * 60;

// Price of the 15 minutes that hold `moment`, or nullptr
inline const PricePoint* priceAt(const AppData& data, time_t moment) {
    for (int i = 0; i < data.priceCount; i++) {
        const PricePoint& p = data.prices[i];
        if (p.start <= moment && moment < p.start + SLOT_SECONDS) {
            return &p;
        }
    }
    return nullptr;
}

inline const PricePoint* priceNow(const AppData& data) {
    return data.pricesValid && data.epoch > 0 ? priceAt(data, data.epoch) : nullptr;
}

// One day of prices (local time)
struct DayPrices {
    bool any = false;
    float hourly[24] = {};   // Average of each hour's quarter-hours
    bool hasHour[24] = {};
    float average = 0;
    float minimum = 0;
    float maximum = 0;
};

inline DayPrices dayPrices(const AppData& data, int32_t day) {
    DayPrices d;
    float sum[24] = {};
    int count[24] = {};
    float total = 0;
    int n = 0;
    for (int i = 0; i < data.priceCount; i++) {
        const PricePoint& p = data.prices[i];
        if (DateUtil::localDay(p.start) != day) {
            continue;
        }
        int hour = DateUtil::localMinute(p.start) / 60;  // The repeated hour in October is merged
        sum[hour] += p.cents;
        count[hour]++;
        total += p.cents;
        if (n == 0 || p.cents < d.minimum) {
            d.minimum = p.cents;
        }
        if (n == 0 || p.cents > d.maximum) {
            d.maximum = p.cents;
        }
        n++;
    }
    for (int h = 0; h < 24; h++) {
        d.hasHour[h] = count[h] > 0;
        d.hourly[h] = count[h] > 0 ? sum[h] / count[h] : 0;
    }
    d.any = n > 0;
    d.average = n > 0 ? total / n : 0;
    return d;
}

// Cheapest run of `hours` whole hours that starts at `fromHour` or later.
// Returns the first hour, or -1 if no such run is left.
inline int cheapestHours(const DayPrices& d, int fromHour, int hours) {
    int best = -1;
    float bestSum = 0;
    for (int h = fromHour < 0 ? 0 : fromHour; h + hours <= 24; h++) {
        float sum = 0;
        bool complete = true;
        for (int k = 0; k < hours && complete; k++) {
            complete = d.hasHour[h + k];
            sum += d.hourly[h + k];
        }
        if (complete && (best < 0 || sum < bestSum)) {
            best = h;
            bestSum = sum;
        }
    }
    return best;
}

// "Cheap now" / "Normal price" / "Expensive now", compared with the day's average
inline const char* levelText(float now, float average) {
    float margin = fabsf(average) * 0.25f;
    if (margin < 0.5f) {
        margin = 0.5f;
    }
    if (now <= average - margin) {
        return "Cheap now";
    }
    if (now >= average + margin) {
        return "Expensive now";
    }
    return "Normal price";
}

// "8.4", "-0.3"; no "-0.0"
inline void formatCents(char* text, size_t size, float cents) {
    snprintf(text, size, "%.1f", fabsf(cents) < 0.05f ? 0.0f : cents);
}

// "02:00-05:00"
inline void formatHours(char* text, size_t size, int firstHour, int hours) {
    snprintf(text, size, "%02d:00-%02d:00", firstHour, (firstHour + hours) % 24);
}

}
