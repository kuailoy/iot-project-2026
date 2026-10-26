#include "MenuPage.h"
#include "../Components.h"

using namespace Components;

static const char* const LABELS[MenuPage::ITEM_COUNT] = {
    "Dashboard", "Pomodoro", "Calendar", "Habits", "Music", "Electricity", "Settings"
};

void MenuPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    // TODO: Replace placeholder values with real data from each mode
    static const char* const values[ITEM_COUNT] = {
        "8C Kuopio", "Ready", "Next 10:30", "3 of 5 done", "Nothing playing", "8.4 c/kWh", ""
    };

    drawHeader(gfx, data);

    const int16_t rowHeight = (FOOTER_LINE_Y - HEADER_LINE_Y - 4) / ITEM_COUNT;
    for (int i = 0; i < ITEM_COUNT; i++) {
        int16_t y = HEADER_LINE_Y + 2 + i * rowHeight;
        bool isLast = (i == ITEM_COUNT - 1);
        drawMenuRow(gfx, y, rowHeight, LABELS[i], values[i], i == selectedItem, !isLast);
    }

    drawFooter(gfx, "Turn: choose      Press: open      Hold: back");
}

void MenuPage::move(int steps) {
    int index = (selectedItem + steps) % ITEM_COUNT;
    if (index < 0) {
        index += ITEM_COUNT;
    }
    selectedItem = static_cast<Item>(index);
}
