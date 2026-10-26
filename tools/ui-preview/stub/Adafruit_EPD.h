#pragma once

// Only the color names, the preview draws into a GFXcanvas1 instead of a real display

#include <Adafruit_GFX.h>

enum {
    EPD_WHITE,
    EPD_BLACK,
    EPD_RED,
    EPD_GRAY,
    EPD_DARK,
    EPD_LIGHT,
    EPD_NUM_COLORS
};
