#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_EPD.h>

#include "../app/AppData.h"

// ---------- Screen layout ----------
// 7.5" E-Ink is 800x480. Figma designs are 1280x800,
// so scale Figma coordinates by x * 0.625, y * 0.6.

constexpr int16_t SCREEN_W = 800;
constexpr int16_t SCREEN_H = 480;
constexpr int16_t MARGIN = 32;
constexpr int16_t HEADER_LINE_Y = 96;
constexpr int16_t FOOTER_LINE_Y = 404;

// ---------- Fonts (defined in Components.cpp) ----------

extern const GFXfont* const FONT_TIME;       // Big clock
extern const GFXfont* const FONT_TEMP;       // Temperatures
extern const GFXfont* const FONT_BODY;       // Normal text, menu rows
extern const GFXfont* const FONT_SMALL;      // Labels
extern const GFXfont* const FONT_SMALL_BOLD; // Date, footer hints

// ---------- Reusable drawing helpers ----------

namespace Components {

void drawText(Adafruit_GFX& gfx, int16_t x, int16_t y, const char* text,
              const GFXfont* font, uint16_t color = EPD_BLACK);
void drawTextRight(Adafruit_GFX& gfx, int16_t rightX, int16_t y, const char* text,
                   const GFXfont* font, uint16_t color = EPD_BLACK);
void drawTextCentered(Adafruit_GFX& gfx, int16_t centerX, int16_t y, const char* text,
                      const GFXfont* font, uint16_t color = EPD_BLACK);

// GFX fonts have no degree sign, so it is drawn as a small circle
void drawTemperature(Adafruit_GFX& gfx, int16_t x, int16_t y, const char* value,
                     const GFXfont* font);

void drawDottedLine(Adafruit_GFX& gfx, int16_t x0, int16_t x1, int16_t y);
void drawThickArc(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius,
                  int16_t startDeg, int16_t endDeg, int16_t thickness);

// ---------- Icons (x, y = top-left corner unless noted) ----------

void drawBatteryIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, int percent); // 41x20
void drawWifiIcon(Adafruit_GFX& gfx, int16_t cx, int16_t bottomY,
                  bool connected);                                      // 36x28, centered on cx
void drawCloudIcon(Adafruit_GFX& gfx, int16_t x, int16_t y);        // 142x84
void drawCalendarIcon(Adafruit_GFX& gfx, int16_t x, int16_t y);     // 46x50

// Clock, date, status icons and the line below them
void drawHeader(Adafruit_GFX& gfx, const AppData& data);

// Line and centered input hints at the bottom of the screen
void drawFooter(Adafruit_GFX& gfx, const char* hints);

// One row of a list, inverted (white on black) when selected
void drawMenuRow(Adafruit_GFX& gfx, int16_t y, int16_t h, const char* label,
                 const char* value, bool selected, bool showDivider);

}
