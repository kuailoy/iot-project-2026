#include "PomodoroPage.h"
#include "../Components.h"

#include <stdio.h>

using namespace Components;

// ---------- Layout (Figma coordinates x 0.625, y 0.6) ----------

static const int16_t RIGHT = SCREEN_W - MARGIN;
static const int16_t STATE_Y = 140;       // "FOCUS", "Session 2 of 4"
static const int16_t NUMBER_Y = 261;      // Baseline of the big number
static const int16_t SIDE_LABEL_Y = 211;  // "ENDS AT"
static const int16_t BAR_Y = 285;
static const int16_t BAR_H = 24;
static const int16_t BOTTOM_LINE_Y = 337;
static const int16_t BOTTOM_LABEL_Y = 358;
static const int16_t BOTTOM_TEXT_Y = 386;

// ---------- Small helpers ----------

// "10:48", or "--:--" when the clock was not synced
static void formatClock(char* buffer, size_t size, int minuteOfDay) {
    if (minuteOfDay < 0) {
        snprintf(buffer, size, "--:--");
    } else {
        snprintf(buffer, size, "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60);
    }
}

// "45 min" or "1h 40m"
static void formatDuration(char* buffer, size_t size, int minutes) {
    if (minutes < 60) {
        snprintf(buffer, size, "%d min", minutes);
    } else {
        snprintf(buffer, size, "%dh %02dm", minutes / 60, minutes % 60);
    }
}

// Filled = done, ring with dot = running now, ring = still to come
static void drawSessionDots(Adafruit_GFX& gfx, const Pomodoro& timer) {
    Pomodoro::State state = timer.state();
    bool focusRunning = state == Pomodoro::FOCUS ||
                        (state == Pomodoro::PAUSED && timer.pausedState() == Pomodoro::FOCUS);

    for (int i = 0; i < Pomodoro::SESSIONS; i++) {
        int16_t cx = RIGHT - 7 - (Pomodoro::SESSIONS - 1 - i) * 22;
        int16_t cy = STATE_Y - 6;

        if (i < timer.completedSessions()) {
            gfx.fillCircle(cx, cy, 8, COLOR_FG);
        } else {
            gfx.fillCircle(cx, cy, 8, COLOR_FG);
            gfx.fillCircle(cx, cy, 6, COLOR_BG);
            if (focusRunning && i == timer.completedSessions()) {
                gfx.fillCircle(cx, cy, 3, COLOR_FG);
            }
        }
    }

    char text[32];
    snprintf(text, sizeof(text), "Session %d of %d", timer.session(), Pomodoro::SESSIONS);
    drawTextRight(gfx, RIGHT - 4 * 22 - 4, STATE_Y, text, FONT_SMALL);
}

// Big number on the left with a small unit after it: "18 min left"
static void drawBigNumber(Adafruit_GFX& gfx, int value, const char* unit) {
    char text[8];
    snprintf(text, sizeof(text), "%d", value);
    drawText(gfx, MARGIN, NUMBER_Y, text, FONT_CLOCK);
    drawText(gfx, gfx.getCursorX() + 16, NUMBER_Y - 4, unit, FONT_BODY);
}

// Label and value on the right: "ENDS AT 10:48"
static void drawSideValue(Adafruit_GFX& gfx, const char* label, const char* value) {
    drawTextRight(gfx, RIGHT, SIDE_LABEL_Y, label, FONT_SMALL);
    drawTextRight(gfx, RIGHT, NUMBER_Y, value, FONT_TEMP);
}

// Row of boxes, the first `filled` ones are black
static void drawSegmentBar(Adafruit_GFX& gfx, int count, int filled, bool rounded) {
    const int16_t width = RIGHT - MARGIN;
    const int16_t gap = (count > 30) ? 2 : (rounded ? 12 : 4);
    const int16_t radius = rounded ? 5 : 0;

    for (int i = 0; i < count; i++) {
        int16_t x0 = MARGIN + i * (width + gap) / count;
        int16_t x1 = MARGIN + (i + 1) * (width + gap) / count - gap;

        if (i < filled) {
            gfx.fillRoundRect(x0, BAR_Y, x1 - x0, BAR_H, radius, COLOR_FG);
        } else {
            gfx.fillRoundRect(x0, BAR_Y, x1 - x0, BAR_H, radius, COLOR_FG);
            gfx.fillRoundRect(x0 + 2, BAR_Y + 2, x1 - x0 - 4, BAR_H - 4, radius, COLOR_BG);
        }
    }
}

// Line plus label/text pairs on the left and the right
static void drawBottomRow(Adafruit_GFX& gfx, const char* leftLabel, const char* leftText,
                          const char* rightLabel, const char* rightText) {
    gfx.fillRect(MARGIN, BOTTOM_LINE_Y, RIGHT - MARGIN, 2, COLOR_FG);

    drawText(gfx, MARGIN, BOTTOM_LABEL_Y, leftLabel, FONT_SMALL);
    drawText(gfx, MARGIN, BOTTOM_TEXT_Y, leftText, FONT_BODY);

    if (rightLabel != nullptr) {
        drawTextRight(gfx, RIGHT, BOTTOM_LABEL_Y, rightLabel, FONT_SMALL);
        drawTextRight(gfx, RIGHT, BOTTOM_TEXT_Y, rightText, FONT_SMALL_BOLD);
    }
}

static void drawPauseIcon(Adafruit_GFX& gfx, int16_t x, int16_t y) {
    gfx.fillRoundRect(x, y, 70, 66, 8, COLOR_FG);
    gfx.fillRect(x + 20, y + 16, 10, 34, COLOR_BG);
    gfx.fillRect(x + 40, y + 16, 10, 34, COLOR_BG);
}

static void drawBellIcon(Adafruit_GFX& gfx, int16_t cx, int16_t top) {
    // Dome, flared sides, rim and clapper, all 3 px lines
    drawThickArc(gfx, cx, top + 26, 26, -180, 0, 3);
    for (int16_t i = -1; i <= 1; i++) {
        gfx.drawLine(cx - 26 + i, top + 26, cx - 34 + i, top + 60, COLOR_FG);
        gfx.drawLine(cx + 26 + i, top + 26, cx + 34 + i, top + 60, COLOR_FG);
    }
    gfx.fillRoundRect(cx - 40, top + 59, 80, 4, 2, COLOR_FG);
    drawThickArc(gfx, cx, top + 66, 7, 0, 180, 3);
}

// ---------- One function per timer state ----------

static void drawReady(Adafruit_GFX& gfx, const Pomodoro& timer) {
    char text[16];
    drawText(gfx, MARGIN, STATE_Y, "READY", FONT_SMALL_BOLD);
    drawSessionDots(gfx, timer);

    drawBigNumber(gfx, timer.focusMinutes(), "min focus");
    snprintf(text, sizeof(text), "%d min", Pomodoro::BREAK_MINUTES);
    drawSideValue(gfx, "BREAK AFTER", text);

    drawSegmentBar(gfx, timer.focusMinutes(), 0, false);
}

static void drawFocus(Adafruit_GFX& gfx, const Pomodoro& timer) {
    char text[16];
    drawText(gfx, MARGIN, STATE_Y, "FOCUS", FONT_SMALL_BOLD);
    drawSessionDots(gfx, timer);

    drawBigNumber(gfx, timer.minutesLeft(), "min left");
    formatClock(text, sizeof(text), timer.endsAtMinute());
    drawSideValue(gfx, "ENDS AT", text);

    drawSegmentBar(gfx, timer.focusMinutes(), timer.elapsedMinutes(), false);
}

static void drawPaused(Adafruit_GFX& gfx, const Pomodoro& timer) {
    char text[16];
    bool isBreak = timer.pausedState() == Pomodoro::BREAK;

    drawText(gfx, MARGIN, STATE_Y, "PAUSED", FONT_SMALL_BOLD);
    drawSessionDots(gfx, timer);

    drawBigNumber(gfx, timer.minutesLeft(), isBreak ? "min of rest left" : "min left");
    drawPauseIcon(gfx, 404, 198);

    // Pause time is shown as a fixed clock time, not a counter,
    // so the screen does not have to refresh while paused
    formatClock(text, sizeof(text), timer.pausedAtMinute());
    drawSideValue(gfx, "PAUSED AT", text);

    drawSegmentBar(gfx, timer.phaseMinutes(), timer.elapsedMinutes(), isBreak);
}

static void drawBreak(Adafruit_GFX& gfx, const Pomodoro& timer) {
    char text[16];
    drawText(gfx, MARGIN, STATE_Y, "SHORT BREAK", FONT_SMALL_BOLD);
    drawSessionDots(gfx, timer);

    drawBigNumber(gfx, timer.minutesLeft(), "min of rest left");
    formatClock(text, sizeof(text), timer.endsAtMinute());
    drawSideValue(gfx, "BACK TO FOCUS", text);

    drawSegmentBar(gfx, Pomodoro::BREAK_MINUTES, timer.elapsedMinutes(), true);
}

static void drawFocusDone(Adafruit_GFX& gfx) {
    char text[32];
    drawBellIcon(gfx, SCREEN_W / 2, 122);
    drawTextCentered(gfx, SCREEN_W / 2, 248, "Focus session done", FONT_TEMP);
    snprintf(text, sizeof(text), "Time for a %d-minute break", Pomodoro::BREAK_MINUTES);
    drawTextCentered(gfx, SCREEN_W / 2, 290, text, FONT_BODY);
}

static void drawCycleDone(Adafruit_GFX& gfx, const Pomodoro& timer, int todayMinutes) {
    char text[16];
    drawText(gfx, MARGIN, STATE_Y, "FOCUSED THIS CYCLE", FONT_SMALL_BOLD);
    drawSessionDots(gfx, timer);

    // "1h 40m": big digits from the clock font, small h and m after them
    int minutes = timer.cycleFocusMinutes();
    gfx.setCursor(MARGIN, NUMBER_Y);
    if (minutes >= 60) {
        snprintf(text, sizeof(text), "%d", minutes / 60);
        drawText(gfx, MARGIN, NUMBER_Y, text, FONT_CLOCK);
        drawText(gfx, gfx.getCursorX() + 6, NUMBER_Y, "h", FONT_TEMP);
        gfx.setCursor(gfx.getCursorX() + 24, NUMBER_Y);
    }
    snprintf(text, sizeof(text), minutes >= 60 ? "%02d" : "%d", minutes % 60);
    drawText(gfx, gfx.getCursorX(), NUMBER_Y, text, FONT_CLOCK);
    drawText(gfx, gfx.getCursorX() + 6, NUMBER_Y, "m", FONT_TEMP);

    formatDuration(text, sizeof(text), todayMinutes);
    drawSideValue(gfx, "FOCUSED TODAY", text);

    // One bar per session with its start time below
    drawSegmentBar(gfx, Pomodoro::SESSIONS, Pomodoro::SESSIONS, false);
    for (int i = 0; i < Pomodoro::SESSIONS; i++) {
        formatClock(text, sizeof(text), timer.sessionStartMinute(i));
        drawText(gfx, MARGIN + i * (RIGHT - MARGIN + 4) / Pomodoro::SESSIONS, BAR_Y + BAR_H + 20,
                 text, FONT_SMALL);
    }
}

// ---------- Page ----------

void PomodoroPage::draw(Adafruit_GFX& gfx, const AppData& data, const Pomodoro& timer,
                        uint32_t nowMs) {
    char today[32];
    char next[32];
    formatDuration(today, sizeof(today), timer.todayFocusMinutes(nowMs));

    bool lastSession = timer.session() == Pomodoro::SESSIONS;
    snprintf(next, sizeof(next), "%d min break", Pomodoro::BREAK_MINUTES);

    drawHeader(gfx, data);

    switch (timer.state()) {
        case Pomodoro::READY:
            drawReady(gfx, timer);
            drawBottomRow(gfx, "FOCUSED TODAY", today, "UP NEXT",
                          lastSession ? "Cycle complete" : next);
            drawFooter(gfx, "Turn: set time      Press: start      Hold: exit");
            break;

        case Pomodoro::FOCUS:
            drawFocus(gfx, timer);
            drawBottomRow(gfx, "FOCUSED TODAY", today, "UP NEXT",
                          lastSession ? "Cycle complete" : next);
            drawFooter(gfx, "Turn: mode      Press: pause");
            break;

        case Pomodoro::PAUSED:
            drawPaused(gfx, timer);
            drawBottomRow(gfx, "FOCUSED TODAY", today, nullptr, nullptr);
            drawFooter(gfx, timer.pausedState() == Pomodoro::BREAK
                                ? "Turn: mode      Press: resume      Hold: skip break"
                                : "Turn: mode      Press: resume      Hold: end session");
            break;

        case Pomodoro::BREAK:
            drawBreak(gfx, timer);
            snprintf(next, sizeof(next), "Focus session %d", timer.session() + 1);
            drawBottomRow(gfx, "FOCUSED TODAY", today, "UP NEXT", next);
            drawFooter(gfx, "Turn: mode      Press: pause      Hold: skip break");
            break;

        case Pomodoro::FOCUS_DONE: {
            drawFocusDone(gfx);
            char done[32];
            snprintf(done, sizeof(done), "Session %d of %d", timer.session(), Pomodoro::SESSIONS);
            drawBottomRow(gfx, "COMPLETED", done, "FOCUSED TODAY", today);
            drawFooter(gfx, "Turn: mode      Press: start break      Hold: skip break");
            break;
        }

        case Pomodoro::CYCLE_DONE:
            drawCycleDone(gfx, timer, timer.todayFocusMinutes(nowMs));
            drawBottomRow(gfx, "WELL DONE", "Take a longer break before the next cycle",
                          nullptr, nullptr);
            drawFooter(gfx, "Turn: mode      Press: new cycle      Hold: exit");
            break;
    }
}
