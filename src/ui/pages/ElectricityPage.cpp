#include "ElectricityPage.h"

#include <stdio.h>

#include "../Components.h"
#include "../../app/DateUtil.h"
#include "../../app/ElectricityData.h"

using namespace Components;
using namespace ElectricityData;

// ---------- Layout (Figma x 0.625, y 0.6) ----------

constexpr int16_t LABEL_Y = 126;        // "SPOT PRICE TODAY"   "Avg 8.3 c/kWh"
constexpr int16_t PRICE_Y = 242;        // Baseline of the big price
constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t CHART_Y = 262;
constexpr int16_t CHART_H = 94;
constexpr int CHEAP_HOURS = 3;

void ElectricityPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    char text[48];
    drawHeader(gfx, data);
    drawFooter(gfx, tomorrow ? "Turn: today      Press: menu      Hold: back"
                             : "Turn: tomorrow      Press: menu      Hold: back");

    if (!data.timeValid) {
        drawTextCentered(gfx, SCREEN_W / 2, 250, "Waiting for the clock to sync", FONT_BODY);
        return;
    }

    int32_t day = DateUtil::dayOf(data.now) + (tomorrow ? 1 : 0);
    DayPrices prices = dayPrices(data, day);

    drawText(gfx, MARGIN, LABEL_Y, tomorrow ? "SPOT PRICE TOMORROW" : "SPOT PRICE TODAY", FONT_SMALL);
    if (!prices.any) {
        const char* message = tomorrow ? "Tomorrow's prices come out in the afternoon"
                            : data.pricesValid ? "No prices for today" : "Loading prices...";
        drawTextCentered(gfx, SCREEN_W / 2, 256, message, FONT_BODY);
        return;
    }
    formatCents(text, sizeof(text), prices.average);
    char line[32];
    snprintf(line, sizeof(line), "Avg %s c/kWh", text);
    drawTextRight(gfx, RIGHT, LABEL_Y, line, FONT_SMALL);

    // Big price: now (today) or the day's average (tomorrow)
    const PricePoint* now = tomorrow ? nullptr : priceNow(data);
    if (tomorrow) {
        formatCents(text, sizeof(text), prices.average);
    } else if (now != nullptr) {
        formatCents(text, sizeof(text), now->cents);
    } else {
        snprintf(text, sizeof(text), "--");
    }
    drawText(gfx, MARGIN, PRICE_Y, text, FONT_CLOCK);
    drawText(gfx, gfx.getCursorX() + 16, PRICE_Y, tomorrow ? "c/kWh average" : "c/kWh now", FONT_BODY);

    // Cheapest 3 hours still ahead (today) or of the whole day (tomorrow)
    int currentHour = tomorrow ? -1 : data.now.tm_hour;
    int cheapest = cheapestHours(prices, tomorrow ? 0 : currentHour, CHEAP_HOURS);
    if (cheapest >= 0) {
        drawTextRight(gfx, RIGHT, 166, "CHEAPEST", FONT_SMALL);
        formatHours(text, sizeof(text), cheapest, CHEAP_HOURS);
        drawTextRight(gfx, RIGHT, 208, text, FONT_TEMP);
        drawTextRight(gfx, RIGHT, 240,
                      cheapest == currentHour ? "Good time for laundry now" : "Good time for laundry or dishes",
                      FONT_SMALL_BOLD);
    }

    drawPriceChart(gfx, MARGIN, CHART_Y, RIGHT - MARGIN, CHART_H, prices.hourly, prices.hasHour, currentHour,
                   cheapest, cheapest >= 0 ? CHEAP_HOURS : 0, true);
}
