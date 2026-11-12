#pragma once

// Minimal Arduino replacement so Adafruit GFX can run on a PC

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Print.h"

typedef bool boolean;

using std::max;

class __FlashStringHelper;

class String : public std::string {
public:
    using std::string::string;
};

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295f
#endif

#define radians(deg) ((deg) * DEG_TO_RAD)

#define PROGMEM

#define constrain(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))
