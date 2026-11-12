#pragma once

#include <Arduino.h>

#include "../app/Calendar.h"

// Downloads an iCal feed over HTTPS and keeps the events between fromDay and toDay.
// "webcal://" links work too, they are the same feed over HTTPS.
// The feed is parsed while it downloads, so its size does not matter.
// Certificate verification is disabled, like the weather request.
bool fetchCalendar(const char* url, int32_t fromDay, int32_t toDay, CalendarData& out,
                   String& error);
#include "../app/AppData.h"

// Downloads an iCal (.ics) calendar feed and lists every event occurrence
// between firstDay and lastDay (local day numbers, see DateUtil.h),
// sorted by time. The feed is parsed while it downloads, so large files
// don't need to fit in RAM.
//
// Supported: single and all-day events, recurring events (RRULE: daily,
// weekly, monthly, yearly with INTERVAL, COUNT, UNTIL, BYDAY, BYMONTHDAY),
// excluded dates (EXDATE), moved or cancelled occurrences (RECURRENCE-ID)
// and cancelled events. Times with a TZID are taken as the device's own
// time zone; UTC times are converted to local time.
//
// Returns the number of events written to `events`, or -1 on failure
// (`error` says why). If there are more than `capacity`, the latest are
// dropped and `completeUntil` is set to the last day whose events are all
// in the list (otherwise it is lastDay).
int fetchCalendar(const char* url, int32_t firstDay, int32_t lastDay,
                  CalendarEvent* events, int capacity, int32_t& completeUntil, String& error);
