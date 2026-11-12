#include "SettingsPage.h"

#include <stdio.h>

#include "../Components.h"

using namespace Components;

// Same QR frame as the setup page
constexpr int16_t QR_BOX_X = 556;
constexpr int16_t QR_BOX_Y = 120;
constexpr int16_t QR_BOX_SIZE = 212;
constexpr int16_t TEXT_X = MARGIN;

static void drawQrCode(Adafruit_GFX& gfx, const AppData& data) {
    gfx.drawRect(QR_BOX_X, QR_BOX_Y, QR_BOX_SIZE, QR_BOX_SIZE, COLOR_FG);
    gfx.drawRect(QR_BOX_X + 1, QR_BOX_Y + 1, QR_BOX_SIZE - 2, QR_BOX_SIZE - 2, COLOR_FG);
    drawTextCentered(gfx, QR_BOX_X + QR_BOX_SIZE / 2, QR_BOX_Y + QR_BOX_SIZE + 26, "Scan to open", FONT_SMALL);

    int size = data.qrSize;
    if (size == 0) {
        return;
    }
    int scale = (QR_BOX_SIZE - 8) / (size + 4);
    int16_t x0 = QR_BOX_X + (QR_BOX_SIZE - size * scale) / 2;
    int16_t y0 = QR_BOX_Y + (QR_BOX_SIZE - size * scale) / 2;
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

void SettingsPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    char text[64];
    drawHeader(gfx, data);
    drawText(gfx, MARGIN, 126, "SETTINGS", FONT_SMALL);

    if (data.deviceIp[0] == '\0') {
        // Not on the home Wi-Fi: only the Wi-Fi setup can help
        drawText(gfx, TEXT_X, 190, "Not connected to Wi-Fi", FONT_BODY);
        drawText(gfx, TEXT_X, 222, "Press the knob to set up Wi-Fi with your phone.", FONT_SMALL);
        drawFooter(gfx, "Press: Wi-Fi setup      Hold: back");
        return;
    }

    drawText(gfx, TEXT_X, 176, "Settings are on your phone", FONT_BODY);
    drawText(gfx, TEXT_X, 204, "Widgets, weather city, calendar, Wi-Fi:", FONT_SMALL);

    drawText(gfx, TEXT_X, 252, "http://desk.local", FONT_TEMP);
    snprintf(text, sizeof(text), "or http://%s", data.deviceIp);
    drawText(gfx, TEXT_X, 284, text, FONT_SMALL_BOLD);

    snprintf(text, sizeof(text), "Phone on the same Wi-Fi: %s", data.wifiName);
    fitText(gfx, text, QR_BOX_X - TEXT_X - 24, FONT_SMALL);
    drawText(gfx, TEXT_X, 336, text, FONT_SMALL);

    drawQrCode(gfx, data);
    drawFooter(gfx, "Hold: back");
}
