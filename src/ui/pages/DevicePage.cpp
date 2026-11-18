#include "DevicePage.h"
#include "../Components.h"
#include "../../app/DeviceReset.h"

#include <Fonts/FreeSans18pt7b.h>
#include <math.h>
#include <stdio.h>

using namespace Components;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t CENTER_X = SCREEN_W / 2;
constexpr int16_t TITLE_Y = 126;      // "DEVICE"
constexpr int16_t FIRST_ROW_Y = 146;  // Top of the first row
constexpr int16_t ROW_H = 49;

// Titles of the confirm screens ("Restart the device?")
static const GFXfont* const FONT_QUESTION = &FreeSans18pt7b;

// ---------- Device page ----------

static bool canChoose(int row) {
    return row == DevicePage::WIFI || row == DevicePage::RESTART || row == DevicePage::RESET;
}

void DevicePage::move(int steps) {
    int row = selectedRow;
    int direction = steps > 0 ? 1 : -1;
    for (int n = steps > 0 ? steps : -steps; n > 0; n--) {
        do {
            row = (row + direction + ROW_COUNT) % ROW_COUNT;
        } while (!canChoose(row));
    }
    selectedRow = static_cast<Row>(row);
}

// Like drawMenuRow(), but rows that only show something have no ">"
static void drawRow(Adafruit_GFX& gfx, int index, const char* label, const char* value, bool selected,
                    bool arrow) {
    const int16_t y = FIRST_ROW_Y + index * ROW_H;
    const int16_t baseline = y + ROW_H - 15;
    const uint16_t color = selected ? COLOR_BG : COLOR_FG;

    if (selected) {
        gfx.fillRoundRect(MARGIN, y + 2, RIGHT - MARGIN, ROW_H - 4, 4, COLOR_FG);
    }
    drawText(gfx, MARGIN + 16, baseline, label, FONT_BODY, color);
    drawTextRight(gfx, arrow ? RIGHT - 40 : RIGHT - 16, baseline, value, FONT_SMALL_BOLD, color);
    if (arrow) {
        drawText(gfx, RIGHT - 24, baseline, ">", FONT_BODY, color);
    }
    if (!selected && index < DevicePage::ROW_COUNT - 1) {
        drawDottedLine(gfx, MARGIN, RIGHT, y + ROW_H - 1);
    }
}

void DevicePage::draw(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);
    drawText(gfx, MARGIN, TITLE_Y, "DEVICE", FONT_SMALL);
    drawTextRight(gfx, RIGHT, TITLE_Y, "More options on desk.local", FONT_SMALL);

    char battery[8];
    snprintf(battery, sizeof(battery), "%d%%", data.batteryPercent);
    const char* wifi = data.wifiConnected && data.wifiName[0] ? data.wifiName
                     : data.wifiConnected                    ? "Connected"
                                                             : "Not connected";

    drawRow(gfx, WIFI, "WiFi", wifi, selectedRow == WIFI, true);
    drawRow(gfx, BATTERY, "Battery", battery, false, false);
    drawRow(gfx, FIRMWARE, "Firmware", FIRMWARE_VERSION, false, false);
    drawRow(gfx, RESTART, "Restart", "", selectedRow == RESTART, true);
    drawRow(gfx, RESET, "Reset and unpair", "", selectedRow == RESET, true);

    drawFooter(gfx, "Turn: choose      Press: select      Hold: back");
}

// ---------- Icons ----------

static int16_t onCircleX(int16_t cx, float r, float deg) {
    return cx + int16_t(lroundf(r * cosf(deg * float(M_PI) / 180.0f)));
}

static int16_t onCircleY(int16_t cy, float r, float deg) {
    return cy + int16_t(lroundf(r * sinf(deg * float(M_PI) / 180.0f)));
}

// Circle open at the top right, with an arrow going round clockwise
static void drawRestartIcon(Adafruit_GFX& gfx, int16_t cx, int16_t cy) {
    const int16_t r = 33;
    drawThickArc(gfx, cx, cy, r, -10, 268, 6);  // 0 = right, -90 = up, clockwise
    // Arrow head at the top end, pointing on round the circle
    const float base = 282, tip = 318;
    gfx.fillTriangle(onCircleX(cx, r - 11, base), onCircleY(cy, r - 11, base), onCircleX(cx, r + 12, base),
                     onCircleY(cy, r + 12, base), onCircleX(cx, r, tip), onCircleY(cy, r, tip), COLOR_FG);
}

// Triangle outline with "!"
static void drawWarningIcon(Adafruit_GFX& gfx, int16_t cx, int16_t top) {
    const int16_t h = 60, half = 36;
    gfx.fillTriangle(cx, top, cx - half, top + h, cx + half, top + h, COLOR_FG);
    gfx.fillTriangle(cx, top + 9, cx - half + 8, top + h - 4, cx + half - 8, top + h - 4, COLOR_BG);
    gfx.fillRect(cx - 2, top + 20, 5, 22, COLOR_FG);
    gfx.fillRect(cx - 2, top + 46, 5, 5, COLOR_FG);
}

// Line above the bottom hints, one hint on each side
static void drawTwoHints(Adafruit_GFX& gfx, const char* left, const char* right) {
    gfx.fillRect(MARGIN, FOOTER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, COLOR_FG);
    drawText(gfx, 92, 446, left, FONT_SMALL_BOLD);
    drawTextRight(gfx, SCREEN_W - 92, 446, right, FONT_SMALL_BOLD);
}

// ---------- Confirm and restarting screens ----------

namespace RestartPages {

void drawRestartConfirm(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);
    drawRestartIcon(gfx, CENTER_X, 157);
    drawTextCentered(gfx, CENTER_X, 240, "Restart the device?", FONT_QUESTION);
    drawTextCentered(gfx, CENTER_X, 275, "Takes about 20 seconds.", FONT_SMALL_BOLD);
    drawTextCentered(gfx, CENTER_X, 299, "Your settings and data are kept.", FONT_SMALL_BOLD);
    drawTwoHints(gfx, "Turn: cancel", "Press: restart");
}

void drawResetConfirm(Adafruit_GFX& gfx, const AppData& data, int filledBoxes) {
    drawHeader(gfx, data);
    drawWarningIcon(gfx, CENTER_X, 114);
    drawTextCentered(gfx, CENTER_X, 216, "Reset and unpair?", FONT_QUESTION);
    drawTextCentered(gfx, CENTER_X, 248, "Clears the WiFi, the settings and the habits on the device.", FONT_SMALL_BOLD);
    drawTextCentered(gfx, CENTER_X, 272, "It starts again with the WiFi setup, like a new one.", FONT_SMALL_BOLD);

    // One box per second the knob is held
    const int16_t boxW = 82, boxH = 22, gap = 8, y = 312;
    const int16_t x0 = CENTER_X - (3 * boxW + 2 * gap) / 2;
    for (int i = 0; i < 3; i++) {
        int16_t x = x0 + i * (boxW + gap);
        gfx.drawRect(x, y, boxW, boxH, COLOR_FG);
        gfx.drawRect(x + 1, y + 1, boxW - 2, boxH - 2, COLOR_FG);
        if (i < filledBoxes) {
            gfx.fillRect(x, y, boxW, boxH, COLOR_FG);
        }
    }
    drawTextCentered(gfx, CENTER_X, 358, "Hold the knob for 3 seconds to reset", FONT_SMALL);
    drawTwoHints(gfx, "Turn: cancel", "Hold 3s: reset");
}

// Done: filled circle with a tick. Now: ring with a dot. Later: dotted ring.
static void drawStep(Adafruit_GFX& gfx, int16_t y, const char* text, int state) {
    const int16_t cx = 294, cy = y - 7, r = 11;
    if (state < 0) {
        gfx.fillCircle(cx, cy, r, COLOR_FG);
        for (int16_t i = 0; i < 3; i++) {
            gfx.drawLine(cx - 6, cy + i - 1, cx - 2, cy + 4 + i - 1, COLOR_BG);
            gfx.drawLine(cx - 2, cy + 4 + i - 1, cx + 6, cy - 4 + i - 1, COLOR_BG);
        }
    } else if (state == 0) {
        gfx.fillCircle(cx, cy, r, COLOR_FG);
        gfx.fillCircle(cx, cy, r - 2, COLOR_BG);
        gfx.fillCircle(cx, cy, 5, COLOR_FG);
    } else {
        for (int deg = 0; deg < 360; deg += 30) {
            gfx.fillCircle(onCircleX(cx, r - 1, deg), onCircleY(cy, r - 1, deg), 1, COLOR_FG);
        }
    }
    drawText(gfx, 320, y, text, state == 0 ? FONT_BODY : FONT_SMALL_BOLD);
}

void drawRestarting(Adafruit_GFX& gfx, bool resetting) {
    drawRestartIcon(gfx, CENTER_X, 104);
    drawTextCentered(gfx, CENTER_X, 196, resetting ? "Resetting" : "Restarting", FONT_TEMP);
    drawTextCentered(gfx, CENTER_X, 228, "This takes about 20 seconds", FONT_SMALL_BOLD);
    gfx.fillRect(CENTER_X - 50, 262, 100, 2, COLOR_FG);

    // What happens next; the screen stays like this until the device is up again
    if (resetting) {
        drawStep(gfx, 306, "Clearing the settings", 0);
        drawStep(gfx, 342, "Starting up", 1);
        drawStep(gfx, 378, "WiFi setup", 1);
    } else {
        drawStep(gfx, 306, "Starting up", 0);
        drawStep(gfx, 342, "Connecting to WiFi", 1);
        drawStep(gfx, 378, "Syncing data", 1);
    }

    char firmware[32];
    snprintf(firmware, sizeof(firmware), "Firmware %s", FIRMWARE_VERSION);
    drawTextCentered(gfx, CENTER_X, 440, firmware, FONT_SMALL);
}

}
