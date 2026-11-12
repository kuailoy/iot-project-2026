#pragma once

#include <stdio.h>

#include "AppData.h"
#include "DateUtil.h"

// Read-only helpers for the calendar events in AppData, used by the pages.
namespace CalendarData {

// Number of events on this day; *first = index of the first one (-1 if none)
inline int eventsOnDay(const AppData& data, int32_t day, int* first) {
    int count = 0;
    *first = -1;
    for (int i = 0; i < data.eventCount; i++) {
        if (data.events[i].day == day) {
            if (count == 0) {
                *first = i;
            }
            count++;
        } else if (data.events[i].day > day) {
            break;
        }
    }
    return count;
}

inline bool hasEvents(const AppData& data, int32_t day) {
    int first;
    return eventsOnDay(data, day, &first) > 0;
}

// The first event that has not ended yet at (day, minute), or nullptr.
// All-day events of the current day are skipped, they would always be "next".
inline const CalendarEvent* nextEvent(const AppData& data, int32_t day, int minute) {
    for (int i = 0; i < data.eventCount; i++) {
        const CalendarEvent& e = data.events[i];
        if (e.day > day) {
            return &e;
        }
        if (e.day == day && !e.allDay && e.endMin > minute) {
            return &e;
        }
    }
    return nullptr;
}

inline const CalendarEvent* nextEvent(const AppData& data) {
    if (!data.timeValid) {
        return nullptr;
    }
    return nextEvent(data, DateUtil::dayOf(data.now), DateUtil::minuteOf(data.now));
}

// "09:00" or "All day"
inline void formatTime(char* buffer, size_t size, const CalendarEvent& e) {
    if (e.allDay) {
        snprintf(buffer, size, "All day");
    } else {
        snprintf(buffer, size, "%02d:%02d", e.startMin / 60, e.startMin % 60);
    }
}

// "10:30 Project meeting" today, "Wed 07 Oct, 09:00 Finnish class" on other days
inline void formatNext(char* buffer, size_t size, const CalendarEvent& e, int32_t today) {
    char time[8];
    formatTime(time, sizeof(time), e);
    if (e.day == today) {
        snprintf(buffer, size, "%s %s", time, e.title);
        return;
    }
    int year, month, day;
    DateUtil::civilFromDays(e.day, year, month, day);
    snprintf(buffer, size, "%s %02d %s, %s %s", DateUtil::WEEKDAY_SHORT[DateUtil::weekday(e.day)], day,
             DateUtil::MONTH_SHORT[month - 1], time, e.title);
}

}
