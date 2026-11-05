#include "Pomodoro.h"

static const uint32_t MS_PER_MINUTE = 60000UL;
static const int MINUTES_PER_DAY = 24 * 60;

// Minute of the day for a clock time plus some milliseconds, -1 if unknown
static int clockMinuteAfter(int32_t clockSec, uint32_t ms) {
    if (clockSec < 0) {
        return -1;
    }
    return ((clockSec + (int32_t)(ms / 1000)) / 60) % MINUTES_PER_DAY;
}

// ---------- Actions ----------

void Pomodoro::adjustFocus(int steps) {
    if (currentState != READY || completed > 0) {
        return;  // Length is fixed once the cycle has started
    }

    focusLength += steps * FOCUS_STEP_MINUTES;
    if (focusLength < MIN_FOCUS_MINUTES) {
        focusLength = MIN_FOCUS_MINUTES;
    }
    if (focusLength > MAX_FOCUS_MINUTES) {
        focusLength = MAX_FOCUS_MINUTES;
    }
}

void Pomodoro::start(uint32_t nowMs, int32_t clockSec) {
    switch (currentState) {
        case READY:
            sessionStart[completed] = clockMinuteAfter(clockSec, 0);
            focusStartMs = nowMs;
            startPhase(FOCUS, focusLength * MS_PER_MINUTE, nowMs, clockSec);
            break;

        case FOCUS_DONE:
            startPhase(BREAK, BREAK_MINUTES * MS_PER_MINUTE, nowMs, clockSec);
            break;

        case CYCLE_DONE:
            completed = 0;
            cycleFocusSec = 0;
            for (int i = 0; i < SESSIONS; i++) {
                sessionStart[i] = -1;
            }
            currentState = READY;
            break;

        default:
            break;
    }
}

void Pomodoro::pause(uint32_t nowMs, int32_t clockSec) {
    if (currentState != FOCUS && currentState != BREAK) {
        return;
    }

    tick(nowMs);
    if (currentState == FOCUS) {
        addFocusTime(nowMs);
    }

    stateBeforePause = currentState;
    pausedAt = clockMinuteAfter(clockSec, 0);
    currentState = PAUSED;
}

void Pomodoro::resume(uint32_t nowMs, int32_t clockSec) {
    if (currentState != PAUSED) {
        return;
    }

    if (stateBeforePause == FOCUS) {
        focusStartMs = nowMs;
    }
    startPhase(stateBeforePause, remainingMs, nowMs, clockSec);
}

void Pomodoro::stop() {
    bool inBreak = currentState == BREAK || currentState == FOCUS_DONE ||
                   (currentState == PAUSED && stateBeforePause == BREAK);
    bool inPausedFocus = currentState == PAUSED && stateBeforePause == FOCUS;

    if (inPausedFocus) {
        // Focus time was already counted when pausing, session is not completed
        currentState = READY;
    } else if (inBreak) {
        currentState = READY;  // Skip the break, next session is ready
    }
}

// ---------- Timer ----------

bool Pomodoro::tick(uint32_t nowMs) {
    if (currentState != FOCUS && currentState != BREAK) {
        return false;
    }

    // Signed difference keeps working when millis() wraps after 49 days
    int32_t left = (int32_t)(phaseEndMs - nowMs);
    remainingMs = left > 0 ? (uint32_t)left : 0;
    if (remainingMs > 0) {
        return false;
    }

    if (currentState == FOCUS) {
        finishFocus(nowMs);
    } else {
        currentState = READY;  // Break over, back to focus
    }
    return true;
}

void Pomodoro::startPhase(State phase, uint32_t durationMs, uint32_t nowMs, int32_t clockSec) {
    currentState = phase;
    remainingMs = durationMs;
    phaseEndMs = nowMs + durationMs;
    endsAt = clockMinuteAfter(clockSec, durationMs);
}

void Pomodoro::addFocusTime(uint32_t nowMs) {
    int32_t seconds = (int32_t)((nowMs - focusStartMs) / 1000);
    todayFocusSec += seconds;
    cycleFocusSec += seconds;
    focusStartMs = nowMs;
}

void Pomodoro::finishFocus(uint32_t nowMs) {
    addFocusTime(nowMs);
    completed++;
    currentState = (completed >= SESSIONS) ? CYCLE_DONE : FOCUS_DONE;
}

// ---------- Read by the page ----------

int Pomodoro::session() const {
    // During the break and its alert, show the session that just ended
    bool afterFocus = currentState == FOCUS_DONE || currentState == BREAK ||
                      (currentState == PAUSED && stateBeforePause == BREAK) ||
                      currentState == CYCLE_DONE;
    int shown = afterFocus ? completed : completed + 1;
    return shown < 1 ? 1 : (shown > SESSIONS ? SESSIONS : shown);
}

int Pomodoro::minutesLeft() const {
    switch (currentState) {
        case READY:
            return focusLength;
        case FOCUS_DONE:
            return BREAK_MINUTES;
        case FOCUS:
        case BREAK:
        case PAUSED:
            return (int)((remainingMs + MS_PER_MINUTE - 1) / MS_PER_MINUTE);
        default:
            return 0;
    }
}

int Pomodoro::phaseMinutes() const {
    bool isBreak = currentState == BREAK || currentState == FOCUS_DONE ||
                   (currentState == PAUSED && stateBeforePause == BREAK);
    return isBreak ? BREAK_MINUTES : focusLength;
}

int Pomodoro::todayFocusMinutes(uint32_t nowMs) const {
    int32_t seconds = todayFocusSec;
    if (currentState == FOCUS) {
        seconds += (int32_t)((nowMs - focusStartMs) / 1000);  // Still running
    }
    return seconds / 60;
}
