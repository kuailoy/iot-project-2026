#include "WidgetLayout.h"

#include <stdio.h>
#include <string.h>

const char* const DEFAULT_WIDGETS = "clock:W,weather:M,indoor:M,next:W";

namespace {

const char* const CODES[WIDGET_TYPE_COUNT] = {"clock", "date", "weather", "indoor", "co2", "next", "agenda"};
const char* const NAMES[WIDGET_TYPE_COUNT] = {"Clock",  "Date", "Weather",    "Indoor",
                                              "CO2",    "Next event", "Today"};
const char SIZE_CODES[SIZE_COUNT] = {'S', 'M', 'L', 'W'};
const uint8_t COLS[SIZE_COUNT] = {1, 2, 2, 4};
const uint8_t ROWS[SIZE_COUNT] = {1, 1, 2, 1};

struct Entry {
    uint8_t type;
    uint8_t size;
};

// "clock:L,weather:M" -> entries; returns the count
int parse(const char* text, Entry* entries, int capacity) {
    int count = 0;
    const char* p = text;
    while (p != nullptr && *p != '\0' && count < capacity) {
        const char* end = strchr(p, ',');
        size_t length = end != nullptr ? size_t(end - p) : strlen(p);
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

int layoutWidgets(const char* text, WidgetSlot* out, int capacity) {
    Entry entries[MAX_WIDGETS];
    int count = parse(text, entries, MAX_WIDGETS);

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
    Entry entries[MAX_WIDGETS];
    int count = parse(text, entries, MAX_WIDGETS);
    size_t length = 0;
    out[0] = '\0';
    for (int i = 0; i < count; i++) {
        int written = snprintf(out + length, outSize - length, "%s%s:%c", i > 0 ? "," : "",
                               CODES[entries[i].type], SIZE_CODES[entries[i].size]);
        if (written < 0 || length + size_t(written) >= outSize) {
            break;
        }
        length += size_t(written);
    }
    return count > 0;
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
