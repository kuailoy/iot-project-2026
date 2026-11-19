#pragma once

#include "AppData.h"
#include "WidgetLayout.h"

// The pages the touch button goes through, in the order of the list on the
// phone: dedicated pages (Home, Calendar, ...) and widget pages, then the
// first one again. No Arduino code here, so the UI preview can test it on a PC.
namespace HomePages {

// The position after `current` in a list of `count` pages; -1 (a page that
// isn't in the list, e.g. one opened from the menu) goes to the first
inline int next(int current, int count) {
    if (count < 1 || current < 0) {
        return 0;
    }
    return (current + 1) % count;
}

// Reads the saved layout text into data: the page list and every widget page, placed
inline void place(AppData& data, const char* text) {
    data.pageCount = readPages(text, data.pageList, MAX_PAGES);
    data.widgetPageCount = 0;
    for (int i = 0; i < data.pageCount; i++) {
        data.widgetPageCount += data.pageList[i].kind == PAGE_WIDGETS;
    }
    for (int grid = 0; grid < MAX_WIDGET_PAGES; grid++) {
        WidgetGrid& page = data.widgetPages[grid];
        page.count = layoutWidgets(text, page.slots, MAX_WIDGETS, grid);
    }
}

}
