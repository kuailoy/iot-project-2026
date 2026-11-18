#include "HabitPage.h"
#include "../Components.h"
#include "../../app/DateUtil.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <stdio.h>
#include <string.h>

using namespace Components;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

constexpr int16_t RIGHT = SCREEN_W - MARGIN;
constexpr int16_t TITLE_Y = 126;        // "LAST 7 DAYS", "3 of 5 done today"
constexpr int16_t DAYS_Y = 172;         // Day letters and "STREAK"
constexpr int16_t NAME_X = 54;
constexpr int16_t NAME_W = 300;         // Up to the first dot column
constexpr int16_t FIRST_COL_X = 375;    // Center of the oldest day
constexpr int16_t COL_STEP = 44;
constexpr int16_t FIRST_ROW_Y = 208;    // Center of the first habit row
constexpr int16_t ROW_STEP = 41;
constexpr int16_t DOT_R = 9;
constexpr int16_t DAYS_SHOWN = 7;

static const GFXfont* const FONT_STREAK = &FreeSansBold12pt7b;

static int16_t columnX(int col) {
    return FIRST_COL_X + col * COL_STEP;
}

static int16_t rowY(int row) {
    return FIRST_ROW_Y + row * ROW_STEP;
}

// Filled = done, ring = not done
static void drawDot(Adafruit_GFX& gfx, int16_t cx, int16_t cy, bool done) {
    gfx.fillCircle(cx, cy, DOT_R, COLOR_FG);
    if (!done) {
        gfx.fillCircle(cx, cy, DOT_R - 2, COLOR_BG);
    }
}

// Day letters, oldest on the left; today is the last column
static void drawDayLetters(Adafruit_GFX& gfx, int32_t today) {
    static const char* const LETTERS[7] = {"M", "T", "W", "T", "F", "S", "S"};
    for (int col = 0; col < DAYS_SHOWN; col++) {
        int32_t day = today - (DAYS_SHOWN - 1 - col);
        drawTextCentered(gfx, columnX(col), DAYS_Y, LETTERS[DateUtil::weekday(day)], FONT_SMALL);
    }
}

// Rounded box around today's column, from the letter down to the last row
static void drawTodayBox(Adafruit_GFX& gfx) {
    const int16_t x = columnX(DAYS_SHOWN - 1) - 19;
    const int16_t y = DAYS_Y - 20;
    const int16_t w = 38;
    const int16_t h = rowY(HABIT_COUNT - 1) + DOT_R + 11 - y;
    gfx.drawRoundRect(x, y, w, h, 5, COLOR_FG);
    gfx.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, COLOR_FG);
}

static void drawHabitRow(Adafruit_GFX& gfx, const Habits& habits, int habit, bool selected) {
    const int16_t cy = rowY(habit);

    if (selected) {
        gfx.fillTriangle(MARGIN, cy - 8, MARGIN, cy + 8, MARGIN + 12, cy, COLOR_FG);
    }

    char name[40];
    strlcpy(name, HABIT_NAMES[habit], sizeof(name));
    fitText(gfx, name, NAME_W, FONT_BODY);
    drawText(gfx, NAME_X, cy + 8, name, FONT_BODY);

    if (habits.known()) {
        for (int col = 0; col < DAYS_SHOWN; col++) {
            drawDot(gfx, columnX(col), cy, habits.done(habit, DAYS_SHOWN - 1 - col));
        }
        char streak[8];
        snprintf(streak, sizeof(streak), "%dd", habits.streak(habit));
        drawTextRight(gfx, RIGHT, cy + 9, streak, FONT_STREAK);
    } else {
        drawTextRight(gfx, RIGHT, cy + 9, "--", FONT_STREAK);
    }
}

void HabitPage::draw(Adafruit_GFX& gfx, const AppData& data, const Habits& habits) {
    drawHeader(gfx, data);

    drawText(gfx, MARGIN, TITLE_Y, "LAST 7 DAYS", FONT_SMALL);
    char summary[32];
    if (habits.known()) {
        snprintf(summary, sizeof(summary), "%d of %d done today", habits.doneToday(), HABIT_COUNT);
    } else {
        snprintf(summary, sizeof(summary), "Waiting for the clock");  // Days need the date
    }
    drawTextRight(gfx, RIGHT, TITLE_Y, summary, FONT_SMALL);

    if (habits.known()) {
        drawDayLetters(gfx, habits.day());
        drawTodayBox(gfx);
    }
    drawTextRight(gfx, RIGHT, DAYS_Y, "STREAK", FONT_SMALL);

    for (int habit = 0; habit < HABIT_COUNT; habit++) {
        drawHabitRow(gfx, habits, habit, habit == selectedHabit);
    }

    drawFooter(gfx, "Turn: mode      Press: check off      Hold: next habit");
}
