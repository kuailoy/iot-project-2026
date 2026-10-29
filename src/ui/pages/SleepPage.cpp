#include "SleepPage.h"
#include "../Components.h"

#include <ctype.h>

using namespace Components;

struct Quote {
    const char* text;
    const char* author;
};

// One quote per day, picked by day of the year
static const Quote QUOTES[] = {
    {"Well begun is half done.", "Aristotle"},
    {"Simplicity is the ultimate sophistication.", "Leonardo da Vinci"},
    {"Little by little, one travels far.", "J.R.R. Tolkien"},
    {"Well done is better than well said.", "Benjamin Franklin"},
    {"The secret of getting ahead is getting started.", "Mark Twain"},
    {"Rest is not idleness.", "John Lubbock"},
    {"What we think, we become.", "Buddha"},
};
static const int QUOTE_COUNT = sizeof(QUOTES) / sizeof(QUOTES[0]);

void SleepPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    const int16_t centerX = SCREEN_W / 2;

    char time[6] = "--:--";
    char date[32] = "";
    if (data.timeValid) {
        strftime(time, sizeof(time), "%H:%M", &data.now);
        strftime(date, sizeof(date), "%A %d %B", &data.now);  // "Monday 05 October"
        for (char* c = date; *c; c++) {
            *c = toupper(*c);
        }
    }

    drawTextCentered(gfx, centerX, 225, time, FONT_CLOCK);
    drawTextCentered(gfx, centerX, 267, date, FONT_SMALL_BOLD);

    gfx.fillRect(centerX - 50, 299, 100, 2, COLOR_FG);

    const Quote& quote = QUOTES[data.now.tm_yday % QUOTE_COUNT];
    drawTextCentered(gfx, centerX, 348, quote.text, FONT_BODY);
    drawTextCentered(gfx, centerX, 380, quote.author, FONT_SMALL);

    drawTextCentered(gfx, centerX, 438, "Press to wake", FONT_SMALL_BOLD);
}
