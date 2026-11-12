#pragma once

#include <Arduino.h>

#include "../app/AppData.h"

// Downloads an iCal (.ics) calendar feed and lists every event occurrence
// between firstDay and lastDay (local day numbers, see DateUtil.h),
// sorted by time. "webcal://" links work too, they are the same feed over
// HTTPS. The feed is parsed while it downloads (IcsParser.h, which also
// lists what is supported), so large files don't need to fit in RAM.
//
// Returns the number of events written to `events`, or -1 on failure
// (`error` says why). If something had to be left out (more than
// `capacity` events, or too many recurring events), `completeUntil` is set
// to the last day whose events are all in the list (otherwise it is lastDay).
int fetchCalendar(const char* url, int32_t firstDay, int32_t lastDay,
                  CalendarEvent* events, int capacity, int32_t& completeUntil, String& error);
