#include "Calendar.h"

#include <string.h>

// ---------- CalendarData ----------

int CalendarData::firstOn(int32_t day) const {
    for (int i = 0; i < count; i++) {
        if (events[i].day == day) {
            return i;
        }
        if (events[i].day > day) {
            break;
        }
    }
    return -1;
}

int CalendarData::countOn(int32_t day) const {
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (events[i].day == day) {
            found++;
        }
    }
    return found;
}

bool CalendarData::sameAs(const CalendarData& other) const {
    if (valid != other.valid || count != other.count) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        const CalendarEvent& a = events[i];
        const CalendarEvent& b = other.events[i];
        if (a.day != b.day || a.startMinute != b.startMinute || a.endMinute != b.endMinute ||
            a.allDay != b.allDay || strcmp(a.title, b.title) != 0) {
            return false;
        }
    }
    return true;
}

const CalendarEvent* CalendarData::nextAfter(int32_t day, int minute) const {
    for (int i = 0; i < count; i++) {
        const CalendarEvent& event = events[i];
        if (event.day > day || (event.day == day && event.endMinute > minute)) {
            return &event;
        }
    }
    return nullptr;
}

// ---------- Dates ----------

namespace CalendarDate {

// Algorithms from http://howardhinnant.github.io/date_algorithms.html
int32_t fromCivil(int year, int month, int dayOfMonth) {
    year -= month <= 2;
    const int32_t era = (year >= 0 ? year : year - 399) / 400;
    const int32_t yearOfEra = year - era * 400;
    const int32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + dayOfMonth - 1;
    const int32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

void toCivil(int32_t day, int* year, int* month, int* dayOfMonth) {
    day += 719468;
    const int32_t era = (day >= 0 ? day : day - 146096) / 146097;
    const int32_t dayOfEra = day - era * 146097;
    const int32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const int32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const int32_t monthIndex = (5 * dayOfYear + 2) / 153;

    *dayOfMonth = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
    *month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
    *year = yearOfEra + era * 400 + (*month <= 2);
}

int32_t fromTm(const struct tm& time) {
    return fromCivil(time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
}

int weekdayOf(int32_t day) {
    // 1970-01-01 was a Thursday
    int weekday = (day + 3) % 7;
    return weekday < 0 ? weekday + 7 : weekday;
}

int daysInMonth(int year, int month) {
    static const int DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return (month == 2 && leap) ? 29 : DAYS[month - 1];
}

int isoWeek(int32_t day) {
    // The week belongs to the year its Thursday is in
    int32_t thursday = day - weekdayOf(day) + 3;
    int year, month, dayOfMonth;
    toCivil(thursday, &year, &month, &dayOfMonth);
    return (thursday - fromCivil(year, 1, 1)) / 7 + 1;
}

const char* monthName(int month) {
    static const char* const NAMES[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return NAMES[(month - 1) % 12];
}

const char* monthShortName(int month) {
    static const char* const NAMES[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    return NAMES[(month - 1) % 12];
}

const char* weekdayShortName(int weekday) {
    static const char* const NAMES[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return NAMES[weekday % 7];
}

}
