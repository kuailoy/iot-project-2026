#pragma once

#include <stdint.h>
#include <time.h>

// Calendar events and date helpers, no Arduino or display code.
// Days are counted from 1970-01-01 in local time, so "tomorrow" is day + 1
// and the week of a day starts at day - weekdayOf(day).

struct CalendarEvent {
    int32_t day = 0;          // Days since 1970-01-01, local time
    int16_t startMinute = 0;  // Minute of the day, 0 for all-day events
    int16_t endMinute = 0;    // 1440 when the event lasts past midnight
    bool allDay = false;
    char title[40] = "";      // Short ASCII title, fits the E-Ink fonts
};

struct CalendarData {
    static const int MAX_EVENTS = 80;
    static const int DAYS_AHEAD = 63;  // Events kept from today, the page can browse this far

    bool valid = false;  // True after the first successful download
    int count = 0;       // Events sorted by day, then start time
    CalendarEvent events[MAX_EVENTS];

    // Index of the first event on that day, -1 if none
    int firstOn(int32_t day) const;
    int countOn(int32_t day) const;

    // Same events, used to skip redraws when a download brought nothing new
    bool sameAs(const CalendarData& other) const;

    // First event that ends after the given day and minute (running or upcoming), or null
    const CalendarEvent* nextAfter(int32_t day, int minute) const;
};

namespace CalendarDate {

// Day number of a date, month 1-12
int32_t fromCivil(int year, int month, int dayOfMonth);
void toCivil(int32_t day, int* year, int* month, int* dayOfMonth);
int32_t fromTm(const struct tm& time);

int weekdayOf(int32_t day);  // 0 = Monday ... 6 = Sunday
int daysInMonth(int year, int month);
int isoWeek(int32_t day);    // Week number used in Finland, 1-53

const char* monthName(int month);       // "October"
const char* monthShortName(int month);  // "Oct"
const char* weekdayShortName(int weekday);  // "Mon", 0 = Monday

}
