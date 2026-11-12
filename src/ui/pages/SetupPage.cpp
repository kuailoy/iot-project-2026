#include "SetupPage.h"

#include <stdio.h>
#include <string.h>

#include "../Components.h"

using namespace Components;

// ---------- Layout (Figma x 0.625, y 0.6) ----------

constexpr int16_t STEP_X = 82;          // Text of the steps
constexpr int16_t NUMBER_X = 50;        // Center of the step number circles
constexpr int16_t QR_BOX_X = 556;       // Frame around the QR code
constexpr int16_t QR_BOX_Y = 120;
constexpr int16_t QR_BOX_SIZE = 212;
constexpr int16_t FOOTER_TEXT_Y = 434;

// Black circle with a white number, centered on (cx, cy)
static void drawStepNumber(Adafruit_GFX& gfx, int16_t cx, int16_t cy, const char* number) {
    gfx.fillCircle(cx, cy, 15, COLOR_FG);
    drawTextCentered(gfx, cx, cy + 6, number, FONT_SMALL_BOLD, COLOR_BG);
}

static void drawQrCode(Adafruit_GFX& gfx, const AppData& data) {
    gfx.drawRect(QR_BOX_X, QR_BOX_Y, QR_BOX_SIZE, QR_BOX_SIZE, COLOR_FG);
    gfx.drawRect(QR_BOX_X + 1, QR_BOX_Y + 1, QR_BOX_SIZE - 2, QR_BOX_SIZE - 2, COLOR_FG);
    drawTextCentered(gfx, QR_BOX_X + QR_BOX_SIZE / 2, QR_BOX_Y + QR_BOX_SIZE + 26, "Scan to join", FONT_SMALL);

    int size = data.qrSize;
    if (size == 0) {
        drawTextCentered(gfx, QR_BOX_X + QR_BOX_SIZE / 2, QR_BOX_Y + QR_BOX_SIZE / 2, "No QR code", FONT_SMALL);
        return;
    }

    // Whole pixels per module, with a white margin of at least 2 modules
    int scale = (QR_BOX_SIZE - 8) / (size + 4);
    int16_t x0 = QR_BOX_X + (QR_BOX_SIZE - size * scale) / 2;
    int16_t y0 = QR_BOX_Y + (QR_BOX_SIZE - size * scale) / 2;

    // QR codes must be dark on light, whatever the UI colors are
    gfx.fillRect(QR_BOX_X + 2, QR_BOX_Y + 2, QR_BOX_SIZE - 4, QR_BOX_SIZE - 4, EPD_WHITE);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int i = y * size + x;
            if (data.qrCode[i / 8] & (0x80 >> (i % 8))) {
                gfx.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, EPD_BLACK);
            }
        }
    }
}

void SetupPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    char text[64];

    drawText(gfx, MARGIN, 72, "Welcome", FONT_TIME);
    drawTextRight(gfx, SCREEN_W - MARGIN, 62, "Wi-Fi setup", FONT_SMALL);
    gfx.fillRect(MARGIN, HEADER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, COLOR_FG);

    // 1. Join the setup Wi-Fi
    drawStepNumber(gfx, NUMBER_X, 133, "1");
    drawText(gfx, STEP_X, 141, "Join the setup Wi-Fi", FONT_BODY);
    snprintf(text, sizeof(text), "Network: %s", data.setupNetwork);
    drawText(gfx, STEP_X, 167, text, FONT_SMALL_BOLD);
    snprintf(text, sizeof(text), "Password: %s", data.setupPassword);
    drawText(gfx, STEP_X, 189, text, FONT_SMALL_BOLD);

    // 2. Wait for the page
    drawStepNumber(gfx, NUMBER_X, 226, "2");
    drawText(gfx, STEP_X, 234, "Wait for the setup page", FONT_BODY);
    drawText(gfx, STEP_X, 260, "Or open 192.168.4.1 in a browser", FONT_SMALL_BOLD);

    // 3. Choose the home Wi-Fi
    drawStepNumber(gfx, NUMBER_X, 302, "3");
    drawText(gfx, STEP_X, 310, "Choose your home Wi-Fi", FONT_BODY);
    drawText(gfx, STEP_X, 336, "Enter its password and save", FONT_SMALL_BOLD);

    drawQrCode(gfx, data);

    // Footer: status on the left, controls on the right
    gfx.fillRect(MARGIN, FOOTER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, COLOR_FG);
    const char* hints = data.setupCanCancel ? "Press: cancel   Hold: restart" : "Hold: restart setup";
    drawTextRight(gfx, SCREEN_W - MARGIN, FOOTER_TEXT_Y, hints, FONT_SMALL_BOLD);
    int16_t hintsWidth = measureText(gfx, hints, FONT_SMALL_BOLD);

    strlcpy(text, data.setupStatus, sizeof(text));
    fitText(gfx, text, SCREEN_W - 2 * MARGIN - hintsWidth - 24, FONT_SMALL);
    drawText(gfx, MARGIN, FOOTER_TEXT_Y, text, FONT_SMALL);
}
