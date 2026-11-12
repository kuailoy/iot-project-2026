#pragma once

#include <Arduino.h>

#include "../app/Calendar.h"

// Downloads an iCal feed over HTTPS and keeps the events between fromDay and toDay.
// "webcal://" links work too, they are the same feed over HTTPS.
// The feed is parsed while it downloads, so its size does not matter.
// Certificate verification is disabled, like the weather request.
bool fetchCalendar(const char* url, int32_t fromDay, int32_t toDay, CalendarData& out,
                   String& error);
