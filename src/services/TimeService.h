#pragma once

#include <Arduino.h>

bool fetchCurrentDateTime(const char* time_api_key, String& date_time, const char* city = "Helsinki");