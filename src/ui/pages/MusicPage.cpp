#include "MusicPage.h"
#include "../Components.h"
#include "../../app/MusicData.h"

#include <stdio.h>
#include <string.h>

using namespace Components;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t ART_Y = 116;       // Square with the note
constexpr int16_t ART_SIZE = 200;
constexpr int16_t TEXT_X = 294;
constexpr int16_t TEXT_W = RIGHT - TEXT_X;
constexpr int16_t BAR_Y = 276;
constexpr int16_t BAR_H = 12;
constexpr int16_t ROW_LINE_Y = 338;  // Line above "LAST PLAYED" / "YOUTUBE MUSIC"

static void drawDashedSquare(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t size) {
    for (int16_t d = 0; d < size; d += 14) {
        int16_t len = d + 8 <= size ? 8 : size - d;
        gfx.fillRect(x + d, y, len, 3, COLOR_FG);
        gfx.fillRect(x + d, y + size - 3, len, 3, COLOR_FG);
        gfx.fillRect(x, y + d, 3, len, COLOR_FG);
        gfx.fillRect(x + size - 3, y + d, 3, len, COLOR_FG);
    }
}

// Text that fits the right column, in the font given
static void drawLine(Adafruit_GFX& gfx, int16_t y, const char* text, const GFXfont* font) {
    char line[64];
    strlcpy(line, text, sizeof(line));
    fitText(gfx, line, TEXT_W, font);
    drawText(gfx, TEXT_X, y, line, font);
}

static void drawSong(Adafruit_GFX& gfx, const AppData& data) {
    char text[16];
    drawText(gfx, TEXT_X, 138, data.musicPlaying ? "NOW PLAYING" : "PAUSED", FONT_SMALL);

    // Title as big as it fits
    bool big = measureText(gfx, data.musicTitle, FONT_TEMP) <= TEXT_W;
    drawLine(gfx, 182, data.musicTitle, big ? FONT_TEMP : FONT_BODY);
    drawLine(gfx, 218, data.musicArtist, FONT_BODY);
    if (data.musicAlbum[0]) {
        drawLine(gfx, 246, data.musicAlbum, FONT_SMALL);
    }

    // Progress, counted on by the display between messages
    int position = MusicData::position(data);
    MusicData::formatTime(text, sizeof(text), position);
    drawText(gfx, TEXT_X, BAR_Y + BAR_H + 22, text, FONT_SMALL);
    if (data.musicDuration > 0) {
        gfx.drawRect(TEXT_X, BAR_Y, TEXT_W, BAR_H, COLOR_FG);
        gfx.drawRect(TEXT_X + 1, BAR_Y + 1, TEXT_W - 2, BAR_H - 2, COLOR_FG);
        gfx.fillRect(TEXT_X, BAR_Y, int32_t(TEXT_W) * position / data.musicDuration, BAR_H, COLOR_FG);
        MusicData::formatTime(text, sizeof(text), data.musicDuration);
        drawTextRight(gfx, RIGHT, BAR_Y + BAR_H + 22, text, FONT_SMALL);
    }
}

static void drawNothing(Adafruit_GFX& gfx, const AppData& data) {
    drawText(gfx, TEXT_X, 138, "NOTHING PLAYING", FONT_SMALL);
    drawText(gfx, TEXT_X, 188, "The room is quiet", FONT_TEMP);
    if (data.musicValid) {
        drawText(gfx, TEXT_X, 230, "Play something in YouTube Music", FONT_SMALL_BOLD);
        drawText(gfx, TEXT_X, 256, "in Chrome and it will show up here.", FONT_SMALL_BOLD);
    } else {
        drawText(gfx, TEXT_X, 230, "Add the browser extension from", FONT_SMALL_BOLD);
        drawText(gfx, TEXT_X, 256, "tools/now-playing-extension, then", FONT_SMALL_BOLD);
        drawText(gfx, TEXT_X, 282, "play something in YouTube Music.", FONT_SMALL_BOLD);
    }
}

void MusicPage::draw(Adafruit_GFX& gfx, const AppData& data) {
    drawHeader(gfx, data);
    drawFooter(gfx, "Press: menu      Hold: back");

    bool active = MusicData::active(data);
    if (active) {
        for (int16_t i = 0; i < 3; i++) {
            gfx.drawRect(MARGIN + i, ART_Y + i, ART_SIZE - 2 * i, ART_SIZE - 2 * i, COLOR_FG);
        }
        drawSong(gfx, data);
    } else {
        drawDashedSquare(gfx, MARGIN, ART_Y, ART_SIZE);
        drawNothing(gfx, data);
    }
    drawMusicIcon(gfx, MARGIN + 30, ART_Y + 30, ART_SIZE - 60, active);

    // Bottom row: the last song while nothing plays; where it comes from
    gfx.fillRect(MARGIN, ROW_LINE_Y, RIGHT - MARGIN, 2, COLOR_FG);
    if (!active && MusicData::hasLast(data)) {
        char last[96];
        drawText(gfx, MARGIN, 360, "LAST PLAYED", FONT_SMALL);
        snprintf(last, sizeof(last), data.musicArtist[0] ? "%s - %s" : "%s", data.musicTitle, data.musicArtist);
        fitText(gfx, last, 470, FONT_BODY);
        drawText(gfx, MARGIN, 388, last, FONT_BODY);
    }
    const char* status = active                 ? (data.musicPlaying ? "Playing" : "Paused")
                       : !data.musicValid       ? "Waiting for the extension"
                       : !data.musicOpen        ? "Tab closed"
                                                : "Not connected";  // Browser closed or asleep
    drawTextRight(gfx, RIGHT, 360, "YOUTUBE MUSIC", FONT_SMALL_BOLD);
    drawTextRight(gfx, RIGHT, 388, status, FONT_BODY);
}
