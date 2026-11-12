#include "Components.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "fonts/ClockFont.h"

const GFXfont* const FONT_CLOCK = &ClockFont;
const GFXfont* const FONT_TIME = &FreeSansBold24pt7b;
const GFXfont* const FONT_TEMP = &FreeSansBold24pt7b;
const GFXfont* const FONT_BODY = &FreeSans12pt7b;
const GFXfont* const FONT_SMALL = &FreeSans9pt7b;
const GFXfont* const FONT_SMALL_BOLD = &FreeSansBold9pt7b;

namespace Components {

// ---------- Text ----------

// Width of the ink, and where it starts relative to the cursor
static uint16_t textWidth(Adafruit_GFX& gfx, const char* text, const GFXfont* font,
                          int16_t* inkStart) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.setFont(font);
    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    *inkStart = x1;
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
    int16_t inkStart;
    uint16_t width = textWidth(gfx, text, font, &inkStart);
    drawText(gfx, rightX - width - inkStart, y, text, font, color);
}

void drawTextCentered(Adafruit_GFX& gfx, int16_t centerX, int16_t y, const char* text,
                      const GFXfont* font, uint16_t color) {
    int16_t inkStart;
    uint16_t width = textWidth(gfx, text, font, &inkStart);
    drawText(gfx, centerX - width / 2 - inkStart, y, text, font, color);
}

int16_t measureText(Adafruit_GFX& gfx, const char* text, const GFXfont* font) {
    int16_t inkStart;
    return textWidth(gfx, text, font, &inkStart);
}

void fitText(Adafruit_GFX& gfx, char* text, int16_t maxWidth, const GFXfont* font) {
    int16_t inkStart;
    if (textWidth(gfx, text, font, &inkStart) <= maxWidth) {
        return;
    }
    // Replace the end with "..", one more character each time until it fits.
    // The result is never longer than the original, so it fits the buffer.
    size_t length = strlen(text);
    while (length >= 2) {
        size_t keep = length - 2;
        while (keep > 0 && text[keep - 1] == ' ') {
            keep--;
        }
        text[keep] = '.';
        text[keep + 1] = '.';
        text[keep + 2] = '\0';
        if (keep == 0 || textWidth(gfx, text, font, &inkStart) <= maxWidth) {
            return;
        }
        length = keep + 1;
    }
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
    gfx.drawCircle(circleX, circleY, radius, COLOR_FG);
    gfx.drawCircle(circleX, circleY, radius - 1, COLOR_FG);

    drawText(gfx, circleX + radius + 3, y, "C", font);
}

// ---------- Shapes and icons ----------

void drawDottedLine(Adafruit_GFX& gfx, int16_t x0, int16_t x1, int16_t y) {
    for (int16_t x = x0; x <= x1; x += 4) {
        gfx.drawPixel(x, y, COLOR_FG);
    }
}

void drawThickArc(Adafruit_GFX& gfx, int16_t cx, int16_t cy, int16_t radius,
                  int16_t startDeg, int16_t endDeg, int16_t thickness) {
    // Angles in degrees, 0 = right, -90 = up
    for (int16_t r = radius - thickness / 2; r <= radius + thickness / 2; r++) {
        for (float deg = startDeg; deg <= endDeg; deg += 0.5f) {
            float rad = deg * DEG_TO_RAD;
            gfx.drawPixel(cx + lroundf(r * cosf(rad)), cy + lroundf(r * sinf(rad)), COLOR_FG);
        }
    }
}

// ---------- Icons ----------

void drawBatteryIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, int percent) {
    percent = constrain(percent, 0, 100);
    gfx.drawRoundRect(x, y, 38, 20, 3, COLOR_FG);
    gfx.drawRoundRect(x + 1, y + 1, 36, 18, 3, COLOR_FG);
    gfx.fillRect(x + 4, y + 4, 30 * percent / 100, 12, COLOR_FG);
    gfx.fillRect(x + 38, y + 6, 3, 8, COLOR_FG);
}

void drawWifiIcon(Adafruit_GFX& gfx, int16_t cx, int16_t bottomY, bool connected) {
    int16_t cy = bottomY - 3;
    gfx.fillCircle(cx, cy, 3, COLOR_FG);
    drawThickArc(gfx, cx, cy, 13, -135, -45, 3);
    drawThickArc(gfx, cx, cy, 24, -135, -45, 3);

    if (!connected) {
        // Diagonal slash through the icon
        for (int16_t i = -1; i <= 1; i++) {
            gfx.drawLine(cx - 16 + i, bottomY - 26, cx + 16 + i, bottomY, COLOR_FG);
        }
    }
}

void drawCloudIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t width) {
    // Shape is designed 142 px wide, s() scales it to the requested width
    auto s = [width](int16_t value) { return (int16_t)(value * width / 142); };

    // Outline = filled shapes in the text color, then the same shapes in the background color, 3 px smaller
    const int16_t stroke = 3;
    for (int pass = 0; pass < 2; pass++) {
        uint16_t color = (pass == 0) ? COLOR_FG : COLOR_BG;
        int16_t inset = (pass == 0) ? 0 : stroke;

        gfx.fillCircle(x + s(32), y + s(52), s(31) - inset, color);       // Left bump
        gfx.fillCircle(x + s(79), y + s(40), s(40) - inset, color);       // Big top bump
        gfx.fillCircle(x + s(110), y + s(52), s(31) - inset, color);      // Right bump
        gfx.fillRect(x + s(32), y + s(52), s(78), s(31) - inset, color);  // Flat bottom
    }
}

void drawCalendarIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, bool check) {
    const int16_t top = y + 6;

    for (int16_t i = 0; i < 3; i++) {
        gfx.drawRoundRect(x + i, top + i, 46 - 2 * i, 44 - 2 * i, 5, COLOR_FG);
    }
    gfx.fillRect(x, top + 11, 46, 3, COLOR_FG);

    // Binder rings
    gfx.fillRect(x + 11, y, 3, 12, COLOR_FG);
    gfx.fillRect(x + 32, y, 3, 12, COLOR_FG);

    // Check mark, 3 px thick
    if (check) {
        for (int16_t i = 0; i < 3; i++) {
            gfx.drawLine(x + 13, y + 32 + i, x + 20, y + 39 + i, COLOR_FG);
            gfx.drawLine(x + 20, y + 39 + i, x + 33, y + 26 + i, COLOR_FG);
        }
    }

}

// ---------- Screen sections ----------

void drawMusicIcon(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t size, bool filled) {
    // Proportions of the note in the Now playing designs (21_ and 22_eink-now-playing-*.png)
    const int16_t r = size * 13 / 100;                                  // Note head radius
    const int16_t stroke = filled ? (size / 24 > 2 ? size / 24 : 2) : 2;
    const int16_t leftX = x + size * 30 / 100, leftY = y + size * 74 / 100;
    const int16_t rightX = x + size * 72 / 100, rightY = y + size * 66 / 100;
    const int16_t leftTop = y + size * 24 / 100, rightTop = y + size * 16 / 100;
    const int16_t leftStem = leftX + r - stroke, rightStem = rightX + r - stroke;
    const int16_t beam = filled ? size * 9 / 100 : stroke;

    if (filled) {
        gfx.fillCircle(leftX, leftY, r, COLOR_FG);
        gfx.fillCircle(rightX, rightY, r, COLOR_FG);
    } else {
        for (int16_t i = 0; i < stroke; i++) {
            gfx.drawCircle(leftX, leftY, r - i, COLOR_FG);
            gfx.drawCircle(rightX, rightY, r - i, COLOR_FG);
        }
    }
    gfx.fillRect(leftStem, leftTop, stroke, leftY - leftTop, COLOR_FG);
    gfx.fillRect(rightStem, rightTop, stroke, rightY - rightTop, COLOR_FG);

    // Slanted beam from the top of one stem to the other
    const int16_t beamRight = rightStem + stroke;
    gfx.fillTriangle(leftStem, leftTop, beamRight, rightTop, beamRight, rightTop + beam, COLOR_FG);
    gfx.fillTriangle(leftStem, leftTop, leftStem, leftTop + beam, beamRight, rightTop + beam, COLOR_FG);
}

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

    gfx.fillRect(MARGIN, HEADER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, COLOR_FG);
}

void drawFooter(Adafruit_GFX& gfx, const char* hints) {
    gfx.fillRect(MARGIN, FOOTER_LINE_Y, SCREEN_W - 2 * MARGIN, 2, COLOR_FG);
    drawTextCentered(gfx, SCREEN_W / 2, 446, hints, FONT_SMALL_BOLD);
}

void drawMenuRow(Adafruit_GFX& gfx, int16_t y, int16_t h, const char* label,
                 const char* value, bool selected, bool showDivider) {
    uint16_t color = selected ? COLOR_BG : COLOR_FG;
    int16_t baseline = y + h - 13;
    int16_t right = SCREEN_W - MARGIN;

    if (selected) {
        gfx.fillRoundRect(MARGIN, y + 2, right - MARGIN, h - 4, 4, COLOR_FG);
    }

    drawText(gfx, MARGIN + 16, baseline, label, FONT_BODY, color);
    drawTextRight(gfx, right - 40, baseline, value, FONT_SMALL_BOLD, color);
    drawText(gfx, right - 24, baseline, ">", FONT_BODY, color);

    if (showDivider && !selected) {
        drawDottedLine(gfx, MARGIN, right, y + h - 1);
    }
}


void drawPriceChart(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                    const float* values, const bool* present, int markerHour,
                    int fillFrom, int fillCount, bool labels) {
    // Scale to the highest bar; negative prices draw as an empty bar
    float top = 0.1f;
    for (int i = 0; i < 24; i++) {
        if (present[i] && values[i] > top) {
            top = values[i];
        }
    }
    const int16_t markerSpace = markerHour >= 0 ? 12 : 0;
    const int16_t barArea = h - markerSpace;
    const int16_t baseline = y + h;
    const float pitch = w / 24.0f;
    const int16_t barW = int16_t(pitch * 0.74f);

    for (int i = 0; i < 24; i++) {
        if (!present[i]) {
            continue;
        }
        int16_t bx = x + int16_t(i * pitch);
        int16_t bh = values[i] > 0 ? int16_t(values[i] / top * barArea + 0.5f) : 0;
        if (bh < 3) {
            bh = 3;  // Still visible
        }
        int16_t by = baseline - bh;
        bool filled = i >= fillFrom && i < fillFrom + fillCount;
        if (filled) {
            gfx.fillRect(bx, by, barW, bh, COLOR_FG);
        } else {
            gfx.drawRect(bx, by, barW, bh, COLOR_FG);
            gfx.drawRect(bx + 1, by + 1, barW - 2, bh - 1, COLOR_FG);
        }
        if (i == markerHour) {
            int16_t cx = bx + barW / 2;
            gfx.fillTriangle(cx - 6, by - 11, cx + 6, by - 11, cx, by - 4, COLOR_FG);
        }
    }
    gfx.fillRect(x, baseline, w, 2, COLOR_FG);

    if (labels) {
        static const int HOURS[] = {0, 6, 12, 18};
        for (int hour : HOURS) {
            char text[4];
            snprintf(text, sizeof(text), "%02d", hour);
            drawText(gfx, x + int16_t(hour * pitch), baseline + 20, text, FONT_SMALL);
        }
        drawTextRight(gfx, x + w, baseline + 20, "23", FONT_SMALL);
    }
}

}
