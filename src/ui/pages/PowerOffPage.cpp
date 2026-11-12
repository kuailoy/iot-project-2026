#include "PowerOffPage.h"
#include "../Components.h"

using namespace Components;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

constexpr int16_t CENTER_X = SCREEN_W / 2;
constexpr int16_t ICON_Y = 128;  // Center of the power symbol
constexpr int16_t ICON_R = 34;

// Power symbol: a ring open at the top with a line through the gap
static void drawPowerIcon(Adafruit_GFX& gfx, int16_t cx, int16_t cy) {
    drawThickArc(gfx, cx, cy, ICON_R, -55, 235, 7);  // 0 = right, -90 = up
    gfx.fillRoundRect(cx - 3, cy - ICON_R - 12, 7, ICON_R + 6, 3, COLOR_FG);
}

void PowerOffPage::draw(Adafruit_GFX& gfx, const struct tm* turnedOffAt) {
    drawPowerIcon(gfx, CENTER_X, ICON_Y);

    drawTextCentered(gfx, CENTER_X, 228, "Turned off", FONT_TEMP);
    drawTextCentered(gfx, CENTER_X, 267, "Touch the button to turn it on.", FONT_BODY);

    if (turnedOffAt != nullptr) {
        char text[32];
        gfx.fillRect(CENTER_X - 50, 299, 100, 2, COLOR_FG);
        drawTextCentered(gfx, CENTER_X, 339, "TURNED OFF AT", FONT_SMALL);
        strftime(text, sizeof(text), "%H:%M, %a %d %b", turnedOffAt);  // "21:14, Mon 05 Oct"
        drawTextCentered(gfx, CENTER_X, 377, text, FONT_SMALL_BOLD);
    }

    // First draw after FIRST_DRAW_DELAY_MS (main.cpp), later if the Wi-Fi is slow
    drawTextCentered(gfx, CENTER_X, 444, "Starting up takes 10 to 20 seconds", FONT_SMALL);
}
