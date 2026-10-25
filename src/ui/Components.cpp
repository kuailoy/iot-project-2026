#include "Components.h"

#include <ctype.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

const GFXfont* const FONT_TIME = &FreeSansBold24pt7b;
const GFXfont* const FONT_TEMP = &FreeSansBold24pt7b;
const GFXfont* const FONT_BODY = &FreeSans12pt7b;
const GFXfont* const FONT_SMALL = &FreeSans9pt7b;
const GFXfont* const FONT_SMALL_BOLD = &FreeSansBold9pt7b;

namespace Components {

// ---------- Text ----------

static uint16_t textWidth(Adafruit_GFX& gfx, const char* text, const GFXfont* font) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.setFont(font);
    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
}

void drawText(Adafruit_GFX& gfx, int16_t x, int16_t y, const char* text,
              const GFXfont* font, uint16_t color) {
    gfx.setFont(font);
    gfx.setTextColor(color);
    gfx.setCursor(x, y);
    gfx.print(text);
}

void drawTextRight(Adafruit_GFX& gfx, int16_t rightX, int16_t y, const char* text,
                   const GFXfont* font, uint16_t color) {
    drawText(gfx, rightX - textWidth(gfx, text, font), y, text, font, color);
}

void drawTextCentered(Adafruit_GFX& gfx, int16_t centerX, int16_t y, const char* text,
                      const GFXfont* font, uint16_t color) {
    drawText(gfx, centerX - textWidth(gfx, text, font) / 2, y, text, font, color);
}

void drawTemperature(Adafruit_GFX& gfx, int16_t x, int16_t y, const char* value,
                     const GFXfont* font) {
    drawText(gfx, x, y, value, font);

    int16_t x1, y1;
    uint16_t w, capHeight;
    gfx.getTextBounds("C", 0, 0, &x1, &y1, &w, &capHeight);

    int16_t radius = capHeight / 6;
    int16_t circleX = gfx.getCursorX() + radius + 3;
    int16_t circleY = y - capHeight + radius;
    gfx.drawCircle(circleX, circleY, radius, EPD_BLACK);
    gfx.drawCircle(circleX, circleY, radius - 1, EPD_BLACK);

    drawText(gfx, circleX + radius + 3, y, "C", font);
}

// ---------- Shapes and icons ----------

void drawDottedLine(Adafruit_GFX& gfx, int16_t x0, int16_t x1, int16_t y) {
    for (int16_t x = x0; x <= x1; x += 4) {
        gfx.drawPixel(x, y, EPD_BLACK);
    }
}

void drawThickArc(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius,
                  int16_t startDeg, int16_t endDeg, int16_t thickness) {
    // Angles in degrees, 0 = right, -90 = up
    for (int16_t r = radius - thickness / 2; r <= radius + thickness / 2; r++) {
        for (float deg = startDeg; deg <= endDeg; deg += 0.5f) {
            float rad = deg * DEG_TO_RAD;
            gfx.drawPixel(cx + lroundf(r * cosf(rad)), cy + lroundf(r * sinf(rad)), EPD_BLACK);
        }
    }
}

// ---------- Icons ----------

void drawBatteryIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, int percent) {
    percent = constrain(percent, 0, 100);
    gfx.drawRoundRect(x, y, 38, 20, 3, EPD_BLACK);
    gfx.drawRoundRect(x + 1, y + 1, 36, 18, 3, EPD_BLACK);
    gfx.fillRect(x + 4, y + 4, 30 * percent / 100, 12, EPD_BLACK);
    gfx.fillRect(x + 38, y + 6, 3, 8, EPD_BLACK);
}

void drawWifiIcon(Adafruit_GFX& gfx, int16_t cx, int16_t bottomY, bool connected) {
    int16_t cy = bottomY - 3;
    gfx.fillCircle(cx, cy, 3, EPD_BLACK);
    drawThickArc(gfx, cx, cy, 13, -135, -45, 3);
    drawThickArc(gfx, cx, cy, 24, -135, -45, 3);

    if (!connected) {
        // Diagonal slash through the icon
        for (int16_t i = -1; i <= 1; i++) {
            gfx.drawLine(cx - 16 + i, bottomY - 26, cx + 16 + i, bottomY, EPD_BLACK);
        }
    }
}

void drawCloudIcon(Adafruit_GFX& gfx, int16_t x, int16_t y) {
    // Outline = black filled shapes, then the same shapes in white, 3 px smaller
    const int16_t stroke = 3;
    for (int pass = 0; pass < 2; pass++) {
        uint16_t color = (pass == 0) ? EPD_BLACK : EPD_WHITE;
        int16_t inset = (pass == 0) ? 0 : stroke;

        gfx.fillCircle(x + 32, y + 52, 31 - inset, color);   // Left bump
        gfx.fillCircle(x + 79, y + 40, 40 - inset, color);   // Big top bump
        gfx.fillCircle(x + 110, y + 52, 31 - inset, color);  // Right bump
        gfx.fillRect(x + 32, y + 52, 78, 31 - inset, color); // Flat bottom
    }
}

void drawCalendarIcon(Adafruit_GFX& gfx, int16_t x, int16_t y) {
    const int16_t top = y + 6;

    for (int16_t i = 0; i < 3; i++) {
        gfx.drawRoundRect(x + i, top + i, 46 - 2 * i, 44 - 2 * i, 5, EPD_BLACK);
    }
    gfx.fillRect(x, top + 11, 46, 3, EPD_BLACK);

    // Binder rings
    gfx.fillRect(x + 11, y, 3, 12, EPD_BLACK);
    gfx.fillRect(x + 32, y, 3, 12, EPD_BLACK);
}

// ---------- Screen sections ----------

void drawHeader(Adafruit_GFX& gfx, const AppData& data) {
    char time[6] = "--:--";
    char date[16] = "";
    if (data.timeValid) {
        strftime(time, sizeof(time), "%H:%M", &data.now);
        strftime(date, sizeof(date), "%a %d %b", &data.now);  // "Mon 05 Oct"
        for (char* c = date; *c; c++) {
            *c = toupper(*c);
        }
    }

    drawText(gfx, MARGIN, 72, time, FONT_TIME);
    drawText(gfx, gfx.getCursorX() + 24, 62, date, FONT_SMALL_BOLD);

    drawWifiIcon(gfx, SCREEN_W - MARGIN - 70, 66, data.wifiConnected);
    drawBatteryIcon(gfx, SCREEN_W - MARGIN - 41, 46, data.batteryPercent);

    gfx.fillRect(MARGIN, HEADER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, EPD_BLACK);
}

void drawFooter(Adafruit_GFX& gfx, const char* hints) {
    gfx.fillRect(MARGIN, FOOTER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, EPD_BLACK);
    drawTextCentered(gfx, SCREEN_W / 2, 446, hints, FONT_SMALL_BOLD);
}

void drawMenuRow(Adafruit_GFX& gfx, int16_t y, int16_t h, const char* label,
                 const char* value, bool selected, bool showDivider) {
    uint16_t color = selected ? EPD_WHITE : EPD_BLACK;
    int16_t baseline = y + h - 13;
    int16_t right = SCREEN_W - MARGIN;

    if (selected) {
        gfx.fillRoundRect(MARGIN, y + 2, right - MARGIN, h - 4, 4, EPD_BLACK);
    }

    drawText(gfx, MARGIN + 16, baseline, label, FONT_BODY, color);
    drawTextRight(gfx, right - 40, baseline, value, FONT_SMALL_BOLD, color);
    drawText(gfx, right - 24, baseline, ">", FONT_BODY, color);

    if (showDivider && !selected) {
        drawDottedLine(gfx, MARGIN, right, y + h - 1);
    }
}

}
