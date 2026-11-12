#pragma once

#include <stddef.h>
#include <stdint.h>

// Widgets of the second home page (opened with the touch button).
//
// The whole page is a grid of 4 x 3 cells (no header or footer), with
// lines between the widgets like on Home. The layout is an
// ordered list such as "agenda:M,claude:M,next:M,co2:S,date:S" (saved in Settings, edited on the
// phone). Widgets are placed in that order, each in the first free spot
// (row by row), like phone home screens. Widgets that don't fit are left out.
//
// No Arduino code here: the UI preview tool builds this on a PC.

enum WidgetType : uint8_t {
    WIDGET_CLOCK,
    WIDGET_DATE,
    WIDGET_WEATHER,
    WIDGET_INDOOR,
    WIDGET_CO2,
    WIDGET_NEXT_EVENT,
    WIDGET_AGENDA,
    WIDGET_CLAUDE,     // Claude plan usage (session and weekly limit)
    WIDGET_PRICE,      // Electricity spot price now and today
    WIDGET_GITHUB,     // GitHub contribution graph
    WIDGET_MUSIC,      // Now playing in YouTube Music (browser extension)
    WIDGET_TYPE_COUNT
};

enum WidgetSize : uint8_t {
    SIZE_S,     // 1 x 1 cells
    SIZE_M,     // 2 x 1
    SIZE_L,     // 2 x 2
    SIZE_WIDE,  // 4 x 1
    SIZE_COUNT
};

constexpr int WIDGET_COLS = 4;
constexpr int WIDGET_ROWS = 3;
constexpr int MAX_WIDGETS = 12;

// Shown until the user saves their own layout
extern const char* const DEFAULT_WIDGETS;

struct WidgetSlot {
    uint8_t type;   // WidgetType
    uint8_t size;   // WidgetSize
    uint8_t col;    // Top-left cell
    uint8_t row;
    uint8_t cols;   // Cells wide and high
    uint8_t rows;
};

// Read a layout text and place the widgets on the grid.
// Returns how many were placed (the rest didn't fit).
int layoutWidgets(const char* text, WidgetSlot* out, int capacity);

// Clean up a layout text: unknown widgets and sizes removed, at most
// MAX_WIDGETS entries. Returns false if nothing valid was left.
bool normalizeWidgets(const char* text, char* out, size_t outSize);

// Names used in the layout text and on the phone page
const char* widgetCode(uint8_t type);    // "clock"
const char* widgetName(uint8_t type);    // "Clock"
char sizeCode(uint8_t size);             // 'S', 'M', 'L', 'W'
uint8_t sizeCols(uint8_t size);
uint8_t sizeRows(uint8_t size);
