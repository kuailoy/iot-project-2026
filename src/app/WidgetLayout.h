#pragma once

#include <stddef.h>
#include <stdint.h>

// The pages the touch button goes through, in order (saved in Settings,
// edited on the phone), separated by "|":
//   "@home|clock:L,weather:M|@calendar|github:W,price:M"
// A page is either a dedicated page with a fixed layout ("@home",
// "@calendar", "@prices", "@music", "@pomodoro"; each at most once) or a
// widget page.
//
// A widget page is a grid of 4 x 3 cells (no header or footer), with lines
// between the widgets like on Home. Its layout is an ordered list such as
// "agenda:M,claude:M,next:M,co2:S,date:S". Widgets are placed in that order,
// each in the first free spot (row by row), like phone home screens. Widgets
// that don't fit are left out.
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
constexpr int MAX_WIDGETS = 12;      // On one page
constexpr int MAX_WIDGET_PAGES = 5;

// Kinds of pages: widget pages, and the dedicated pages with a fixed layout
enum PageKind : uint8_t {
    PAGE_WIDGETS,
    PAGE_HOME,       // "@home": clock, weather, indoor, next event
    PAGE_CALENDAR,   // "@calendar"
    PAGE_PRICES,     // "@prices": electricity spot price
    PAGE_MUSIC,      // "@music": now playing
    PAGE_POMODORO,   // "@pomodoro": focus timer
    PAGE_KIND_COUNT
};

// Every dedicated page once, and the widget pages
constexpr int MAX_PAGES = MAX_WIDGET_PAGES + PAGE_KIND_COUNT - 1;

// Longest layout text: full widget pages ("weather:W," is the longest entry)
// and every dedicated page ("@pomodoro|" is the longest)
constexpr size_t MAX_LAYOUT_TEXT = MAX_WIDGET_PAGES * (MAX_WIDGETS * 10 + 1) + (PAGE_KIND_COUNT - 1) * 10 + 1;

// One widget page, shown until the user saves their own layout
extern const char* const DEFAULT_WIDGETS;
// Home, then DEFAULT_WIDGETS
extern const char* const DEFAULT_PAGES;

// A page of the list. grid: for a widget page, which one (0 = the first)
struct PageEntry {
    uint8_t kind = PAGE_HOME;
    uint8_t grid = 0;
};

struct WidgetSlot {
    uint8_t type;   // WidgetType
    uint8_t size;   // WidgetSize
    uint8_t col;    // Top-left cell
    uint8_t row;
    uint8_t cols;   // Cells wide and high
    uint8_t rows;
};

// Read widget page `page` (0 = the first widget page, dedicated pages are
// skipped) and place its widgets on the grid. Returns how many were placed
// (the rest didn't fit); 0 for a missing page.
int layoutWidgets(const char* text, WidgetSlot* out, int capacity, int page = 0);

// Widget pages in a layout text, at most MAX_WIDGET_PAGES. An empty text is
// one empty widget page; "@home" alone has none.
int widgetPageCount(const char* text);

// The pages of a layout text in order, at most `capacity`. Unknown and
// repeated dedicated pages are skipped. Returns the count, at least 1 (with
// nothing valid: one empty widget page).
int readPages(const char* text, PageEntry* out, int capacity);

// Clean up a layout text: unknown widgets, sizes and pages removed, each
// dedicated page once, at most MAX_WIDGETS entries a page and
// MAX_WIDGET_PAGES widget pages (empty widget pages stay).
// Returns false if no page has a valid widget.
bool normalizeWidgets(const char* text, char* out, size_t outSize);

// Dedicated pages: "home" (written "@home"), "Home"; "" for PAGE_WIDGETS
const char* pageCode(uint8_t kind);
const char* pageName(uint8_t kind);

// Names used in the layout text and on the phone page
const char* widgetCode(uint8_t type);    // "clock"
const char* widgetName(uint8_t type);    // "Clock"
char sizeCode(uint8_t size);             // 'S', 'M', 'L', 'W'
uint8_t sizeCols(uint8_t size);
uint8_t sizeRows(uint8_t size);
