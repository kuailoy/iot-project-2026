#pragma once

#include <stdint.h>
#include <time.h>

// Calendar dates as whole days since 1970-01-01 ("day numbers").
// They are local dates without a time zone, so adding 1 is always the next
// day and the difference of two dates is the number of days between them.
// Conversion algorithms: Howard Hinnant, "chrono-compatible low-level date algorithms".
namespace DateUtil {

inline int32_t daysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const int32_t era = (year >= 0 ? year : year - 399) / 400;
    const uint32_t yearOfEra = uint32_t(year - era * 400);
    const uint32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + int32_t(dayOfEra) - 719468;
}

inline void civilFromDays(int32_t days, int& year, int& month, int& day) {
    days += 719468;
    const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
    const uint32_t dayOfEra = uint32_t(days - era * 146097);
    const uint32_t yearOfEra =
        (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const uint32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const uint32_t mp = (5 * dayOfYear + 2) / 153;
    day = int(dayOfYear - (153 * mp + 2) / 5 + 1);
    month = int(mp < 10 ? mp + 3 : mp - 9);
    year = int(yearOfEra) + era * 400 + (month <= 2);
}

// Monday = 0 ... Sunday = 6 (1970-01-01 was a Thursday)
inline int weekday(int32_t days) {
    return int(((days % 7) + 7 + 3) % 7);
}

inline bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

inline int daysInMonth(int year, int month) {
    static const uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (month == 2 && isLeapYear(year)) ? 29 : DAYS[month - 1];
}

inline int32_t dayOf(const struct tm& t) {
    return daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

inline int minuteOf(const struct tm& t) {
    return t.tm_hour * 60 + t.tm_min;
}

// ISO 8601 week number: weeks start on Monday, week 1 holds the first Thursday
inline int isoWeek(int32_t days) {
    int32_t thursday = days - weekday(days) + 3;
    int year, month, day;
    civilFromDays(thursday, year, month, day);
    return int((thursday - daysFromCivil(year, 1, 1)) / 7) + 1;
}

// "2026-11-04T17:30:00.294921+00:00", "...+02:00" or "...Z" -> seconds since 1970 (UTC), 0 if invalid
inline time_t parseIsoTime(const char* text) {
    auto digits = [](const char* p, int count, int& out) -> bool {
        out = 0;
        for (int i = 0; i < count; i++) {
            if (p[i] < '0' || p[i] > '9') {
                return false;
            }
            out = out * 10 + (p[i] - '0');
        }
        return true;
    };
    if (text == nullptr) {
        return 0;
    }
    for (int i = 0; i < 19; i++) {
        if (text[i] == '\0') {
            return 0;
        }
    }
    int year, month, day, hour, minute, second;
    if (!digits(text, 4, year) || text[4] != '-' || !digits(text + 5, 2, month) || text[7] != '-' ||
        !digits(text + 8, 2, day) || text[10] != 'T' || !digits(text + 11, 2, hour) ||
        !digits(text + 14, 2, minute) || !digits(text + 17, 2, second)) {
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
        if (!digits(p + 1, 2, offsetHours) || p[3] != ':' || !digits(p + 4, 2, offsetMinutes)) {
            return 0;
        }
        offset = (offsetHours * 60L + offsetMinutes) * 60L * (*p == '-' ? -1 : 1);
    }
    long long days = daysFromCivil(year, month, day);
    return time_t(days * 86400LL + hour * 3600LL + minute * 60LL + second - offset);
}

// Local date (day number) and minutes after midnight of a moment, in the device's time zone
inline int32_t localDay(time_t moment) {
    struct tm local;
    localtime_r(&moment, &local);
    return dayOf(local);
}

inline int localMinute(time_t moment) {
    struct tm local;
    localtime_r(&moment, &local);
    return minuteOf(local);
}

static const char* const MONTH_NAMES[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char* const MONTH_SHORT[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char* const WEEKDAY_SHORT[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

}
