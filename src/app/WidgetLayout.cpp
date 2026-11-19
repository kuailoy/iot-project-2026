#include "WidgetLayout.h"

#include <stdio.h>
#include <string.h>

const char* const DEFAULT_WIDGETS = "agenda:M,claude:M,next:M,co2:S,date:S,price:M,weather:M";
const char* const DEFAULT_PAGES = "@home|agenda:M,claude:M,next:M,co2:S,date:S,price:M,weather:M";

namespace {

const char* const CODES[WIDGET_TYPE_COUNT] = {"clock", "date", "weather", "indoor", "co2",
                                              "next",  "agenda", "claude", "price", "github", "music"};
const char* const NAMES[WIDGET_TYPE_COUNT] = {"Clock",      "Date",  "Weather", "Indoor",      "CO2",
                                              "Next event", "Today", "Claude",  "Electricity", "GitHub",
                                              "Now playing"};
const char* const PAGE_CODES[PAGE_KIND_COUNT] = {"", "home", "calendar", "prices", "music", "pomodoro"};
const char* const PAGE_NAMES[PAGE_KIND_COUNT] = {"Widgets", "Home", "Calendar", "Electricity", "Music", "Pomodoro"};
const char SIZE_CODES[SIZE_COUNT] = {'S', 'M', 'L', 'W'};

// Pages read from a text at most; more "|" than this are ignored
constexpr int MAX_SEGMENTS = 32;
const uint8_t COLS[SIZE_COUNT] = {1, 2, 2, 4};
const uint8_t ROWS[SIZE_COUNT] = {1, 1, 2, 1};

struct Entry {
    uint8_t type;
    uint8_t size;
};

// Start of the page after `segment` (which ends at "|" or the end), or nullptr
const char* nextSegment(const char* segment) {
    const char* bar = strchr(segment, '|');
    return bar != nullptr ? bar + 1 : nullptr;
}

// A dedicated page: "@home" (up to "|" or the end) -> PAGE_HOME, else PAGE_WIDGETS
uint8_t dedicatedKind(const char* segment) {
    if (segment[0] != '@') {
        return PAGE_WIDGETS;
    }
    const char* end = strchr(segment, '|');
    size_t length = (end != nullptr ? size_t(end - segment) : strlen(segment)) - 1;
    for (uint8_t kind = PAGE_HOME; kind < PAGE_KIND_COUNT; kind++) {
        if (strlen(PAGE_CODES[kind]) == length && strncmp(segment + 1, PAGE_CODES[kind], length) == 0) {
            return kind;
        }
    }
    return PAGE_KIND_COUNT;  // Unknown
}

// Start of widget page `page` (dedicated pages don't count), or nullptr
const char* pageStart(const char* text, int page) {
    if (text == nullptr || page < 0 || page >= MAX_WIDGET_PAGES) {
        return nullptr;
    }
    int widgetPage = 0;
    const char* segment = text;
    for (int i = 0; i < MAX_SEGMENTS && segment != nullptr; i++, segment = nextSegment(segment)) {
        if (segment[0] == '@') {
            continue;
        }
        if (widgetPage == page) {
            return segment;
        }
        widgetPage++;
    }
    return nullptr;
}

// One page, "clock:L,weather:M" (up to "|" or the end) -> entries; returns the count
int parse(const char* text, Entry* entries, int capacity) {
    int count = 0;
    const char* pageEnd = text != nullptr ? strchr(text, '|') : nullptr;
    if (pageEnd == nullptr && text != nullptr) {
        pageEnd = text + strlen(text);
    }
    const char* p = text;
    while (p != nullptr && p < pageEnd && count < capacity) {
        const char* end = static_cast<const char*>(memchr(p, ',', size_t(pageEnd - p)));
        size_t length = end != nullptr ? size_t(end - p) : size_t(pageEnd - p);
        const char* colon = static_cast<const char*>(memchr(p, ':', length));

        if (colon != nullptr && colon + 1 < p + length) {
            size_t codeLength = size_t(colon - p);
            char size = colon[1];
            for (uint8_t t = 0; t < WIDGET_TYPE_COUNT; t++) {
                if (strlen(CODES[t]) == codeLength && strncmp(p, CODES[t], codeLength) == 0) {
                    for (uint8_t s = 0; s < SIZE_COUNT; s++) {
                        if (SIZE_CODES[s] == size) {
                            entries[count++] = {t, s};
                        }
                    }
                    break;
                }
            }
        }
        p = end != nullptr ? end + 1 : nullptr;
    }
    return count;
}

}  // namespace

int widgetPageCount(const char* text) {
    int pages = 0;
    while (pages < MAX_WIDGET_PAGES && pageStart(text, pages) != nullptr) {
        pages++;
    }
    return pages;
}

int readPages(const char* text, PageEntry* out, int capacity) {
    int count = 0;
    uint8_t grids = 0;
    bool seen[PAGE_KIND_COUNT] = {};
    const char* segment = text;
    for (int i = 0; i < MAX_SEGMENTS && segment != nullptr && count < capacity; i++, segment = nextSegment(segment)) {
        uint8_t kind = dedicatedKind(segment);
        if (kind == PAGE_WIDGETS && grids < MAX_WIDGET_PAGES) {
            out[count].kind = PAGE_WIDGETS;
            out[count++].grid = grids++;
        } else if (kind != PAGE_WIDGETS && kind < PAGE_KIND_COUNT && !seen[kind]) {
            seen[kind] = true;
            out[count].kind = kind;
            out[count++].grid = 0;
        }
    }
    if (count == 0 && capacity > 0) {
        out[0].kind = PAGE_WIDGETS;  // Nothing valid: one empty widget page
        out[0].grid = 0;
        count = 1;
    }
    return count;
}

int layoutWidgets(const char* text, WidgetSlot* out, int capacity, int page) {
    Entry entries[MAX_WIDGETS];
    int count = parse(pageStart(text, page), entries, MAX_WIDGETS);

    bool used[WIDGET_ROWS][WIDGET_COLS] = {};
    int placed = 0;
    for (int i = 0; i < count && placed < capacity; i++) {
        uint8_t w = COLS[entries[i].size];
        uint8_t h = ROWS[entries[i].size];

        // First free spot, row by row
        bool found = false;
        for (int row = 0; row + h <= WIDGET_ROWS && !found; row++) {
            for (int col = 0; col + w <= WIDGET_COLS && !found; col++) {
                bool free = true;
                for (int r = row; r < row + h && free; r++) {
                    for (int c = col; c < col + w; c++) {
                        free &= !used[r][c];
                    }
                }
                if (!free) {
                    continue;
                }
                for (int r = row; r < row + h; r++) {
                    for (int c = col; c < col + w; c++) {
                        used[r][c] = true;
                    }
                }
                out[placed++] = {entries[i].type, entries[i].size, uint8_t(col), uint8_t(row), w, h};
                found = true;
            }
        }
    }
    return placed;
}

bool normalizeWidgets(const char* text, char* out, size_t outSize) {
    size_t length = 0;
    out[0] = '\0';
    bool any = false;
    int pages = 0;
    int widgetPages = 0;
    bool seen[PAGE_KIND_COUNT] = {};

    // Appends a whole piece, or nothing and false when it doesn't fit
    auto append = [&](const char* piece) {
        size_t pieceLength = strlen(piece);
        if (length + pieceLength >= outSize) {
            return false;
        }
        memcpy(out + length, piece, pieceLength + 1);
        length += pieceLength;
        return true;
    };
    char piece[24];

    const char* segment = text;
    for (int i = 0; i < MAX_SEGMENTS && segment != nullptr; i++, segment = nextSegment(segment)) {
        uint8_t kind = dedicatedKind(segment);
        if (kind == PAGE_KIND_COUNT || (kind != PAGE_WIDGETS && seen[kind]) ||
            (kind == PAGE_WIDGETS && widgetPages >= MAX_WIDGET_PAGES)) {
            continue;  // Unknown, repeated, or one widget page too many
        }
        // The "|" before every page but the first
        if (pages > 0 && !append("|")) {
            return any;
        }
        pages++;
        if (kind != PAGE_WIDGETS) {
            seen[kind] = true;
            snprintf(piece, sizeof(piece), "@%s", PAGE_CODES[kind]);
            if (!append(piece)) {
                return any;
            }
            continue;
        }
        Entry entries[MAX_WIDGETS];
        int count = parse(segment, entries, MAX_WIDGETS);
        any |= count > 0;
        widgetPages++;
        for (int e = 0; e < count; e++) {
            snprintf(piece, sizeof(piece), "%s%s:%c", e > 0 ? "," : "", CODES[entries[e].type],
                     SIZE_CODES[entries[e].size]);
            if (!append(piece)) {
                return any;
            }
        }
    }
    return any;
}

const char* pageCode(uint8_t kind) {
    return kind < PAGE_KIND_COUNT ? PAGE_CODES[kind] : "";
}

const char* pageName(uint8_t kind) {
    return kind < PAGE_KIND_COUNT ? PAGE_NAMES[kind] : "";
}

const char* widgetCode(uint8_t type) {
    return type < WIDGET_TYPE_COUNT ? CODES[type] : "";
}

const char* widgetName(uint8_t type) {
    return type < WIDGET_TYPE_COUNT ? NAMES[type] : "";
}

char sizeCode(uint8_t size) {
    return size < SIZE_COUNT ? SIZE_CODES[size] : 'S';
}

uint8_t sizeCols(uint8_t size) {
    return size < SIZE_COUNT ? COLS[size] : 1;
}

uint8_t sizeRows(uint8_t size) {
    return size < SIZE_COUNT ? ROWS[size] : 1;
}
