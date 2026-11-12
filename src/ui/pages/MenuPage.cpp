#include "MenuPage.h"
#include "../Components.h"
#include "../../app/CalendarData.h"
#include "../../app/ElectricityData.h"
#include "../../app/MusicData.h"

#include <stdio.h>
#include <string.h>

using namespace Components;

static const char* const LABELS[MenuPage::ITEM_COUNT] = {
    "Dashboard", "Pomodoro", "Calendar", "Habits", "Music", "Electricity", "Settings"
};

void MenuPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    // TODO: Replace placeholder values with real data from each mode
    static const char* const values[ITEM_COUNT] = {
        "8C Kuopio", "Ready", "", "3 of 5 done", "Nothing playing", "", ""
    };

    // Calendar row: "Next 10:30" today, "Next Wed 07" later
    char calendarValue[20] = "--";
    const CalendarEvent* next = CalendarData::nextEvent(data);
    if (next != nullptr) {
        int year, month, day;
        DateUtil::civilFromDays(next->day, year, month, day);
        if (next->day == DateUtil::dayOf(data.now) && !next->allDay) {
            snprintf(calendarValue, sizeof(calendarValue), "Next %02d:%02d", next->startMin / 60, next->startMin % 60);
        } else {
            snprintf(calendarValue, sizeof(calendarValue), "Next %s %02d",
                     DateUtil::WEEKDAY_SHORT[DateUtil::weekday(next->day)], day);
        }
    } else if (data.calendarValid) {
        snprintf(calendarValue, sizeof(calendarValue), "No events");
    }

    // Music row: the song, or "Paused" / "Nothing playing"
    char musicValue[24] = "Nothing playing";
    if (MusicData::active(data)) {
        if (data.musicPlaying) {
            strlcpy(musicValue, data.musicTitle, sizeof(musicValue));
        } else {
            snprintf(musicValue, sizeof(musicValue), "Paused");
        }
    }

    // Electricity row: the spot price now
    char priceValue[20] = "--";
    const PricePoint* price = ElectricityData::priceNow(data);
    if (price != nullptr) {
        char cents[12];
        ElectricityData::formatCents(cents, sizeof(cents), price->cents);
        snprintf(priceValue, sizeof(priceValue), "%s c/kWh", cents);
    }

    drawHeader(gfx, data);

    const int16_t rowHeight = (FOOTER_LINE_Y - HEADER_LINE_Y - 4) / ITEM_COUNT;
    for (int i = 0; i < ITEM_COUNT; i++) {
        int16_t y = HEADER_LINE_Y + 2 + i * rowHeight;
        bool isLast = (i == ITEM_COUNT - 1);
        const char* value = i == CALENDAR    ? calendarValue
                          : i == ELECTRICITY ? priceValue
                          : i == MUSIC       ? musicValue
                                             : values[i];
        drawMenuRow(gfx, y, rowHeight, LABELS[i], value, i == selectedItem, !isLast);
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
