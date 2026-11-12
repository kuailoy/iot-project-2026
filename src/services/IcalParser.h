#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/Calendar.h"

// Reads an iCalendar (.ics) feed piece by piece, so the whole file never
// has to fit in memory. Keeps the events between fromDay and toDay.
//
// Supported: DTSTART/DTEND as local time, UTC ("Z") or all-day dates,
// folded lines, cancelled events. Times with another TZID are treated as
// local time. Repeating events (RRULE) only show their first date.
class IcalParser {
public:
    // Clears `out`, then fills it while data is fed in
    IcalParser(CalendarData& out, int32_t fromDay, int32_t toDay);

    void feed(const char* data, size_t length);
    void feed(char c);

    // Call after the last byte, handles the last line
    void finish();

    int eventsSeen() const { return seen; }  // All VEVENTs, also outside the range

private:
    static const int LINE_SIZE = 200;  // Longer lines are cut, enough for titles

    CalendarData& calendar;
    int32_t firstDay;
    int32_t lastDay;  // Exclusive

    char line[LINE_SIZE];
    int lineLength = 0;
    bool lineEnded = false;  // Newline seen, the next char decides about folding

    bool inEvent = false;
    int nestedDepth = 0;  // VALARM etc. inside an event
    bool hasStart = false;
    bool hasEnd = false;
    bool cancelled = false;
    int32_t endDay = 0;
    CalendarEvent current;
    int seen = 0;

    void handleLine();
    void handleProperty(const char* name, const char* value);
    void finishEvent();
    void addEvent(const CalendarEvent& event);
};
