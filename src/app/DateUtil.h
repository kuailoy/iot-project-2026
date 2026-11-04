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

static const char* const MONTH_NAMES[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char* const MONTH_SHORT[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char* const WEEKDAY_SHORT[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

}
