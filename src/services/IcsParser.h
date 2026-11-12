#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/AppData.h"

// Reads an iCal (.ics) file and lists every event occurrence between
// firstDay and lastDay (local day numbers, see DateUtil.h), sorted by time.
// The file is fed in pieces while it downloads, so it doesn't need to fit
// in RAM. No Arduino or network code, so the UI preview can test it on a PC.
//
// Supported: single and all-day events, recurring events (RRULE: daily,
// weekly, monthly, yearly with INTERVAL, COUNT, UNTIL, BYDAY, BYMONTHDAY),
// excluded dates (EXDATE), moved or cancelled occurrences (RECURRENCE-ID)
// and cancelled events. Times with a TZID are taken as the device's own
// time zone; UTC times are converted to local time.
class IcsParser {
public:
    // Limits; what is over them is left out and completeUntil() says which
    // days can't be trusted then
    static constexpr int MAX_RECURRING = 48;  // Recurring events that reach the shown days
    static constexpr int MAX_OVERRIDES = 64;  // Moved or cancelled single occurrences

    // About 18 KB, so it goes on the heap only while reading; nullptr if
    // there is not enough RAM. Events are written to events[0 .. capacity-1].
    static IcsParser* create(int32_t firstDay, int32_t lastDay, CalendarEvent* events, int capacity);
    virtual ~IcsParser() = default;

    // The next piece of the file, any size
    virtual void feed(const uint8_t* data, size_t size) = 0;

    // Call after the last piece: handles the last line and expands recurring events
    virtual void finish() = 0;

    virtual bool sawCalendar() const = 0;  // The file had a VCALENDAR
    virtual int count() const = 0;         // Events written
    virtual int dropped() const = 0;       // Occurrences, series and changes left out

    // Last day whose events are all in the list and correct (lastDay if
    // nothing was left out). The days after it are unknown, not free.
    virtual int32_t completeUntil() const = 0;
};
