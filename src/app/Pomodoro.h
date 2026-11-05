#pragma once

#include <stdint.h>

// Pomodoro timer logic, no drawing and no Arduino calls.
// Times come in as nowMs (millis()) and clockSec (seconds since
// midnight, -1 when the clock is not synced yet).
//
// Flow: READY -> FOCUS -> FOCUS_DONE -> BREAK -> READY (next session)
//       After the last session: FOCUS -> CYCLE_DONE
//       FOCUS and BREAK can be PAUSED.
class Pomodoro {
public:
    enum State {
        READY,       // Waiting to start, focus length can be changed
        FOCUS,
        PAUSED,
        FOCUS_DONE,  // Alert: time for a break
        BREAK,
        CYCLE_DONE   // All sessions of the cycle done
    };

    static const int SESSIONS = 4;
    static const int BREAK_MINUTES = 5;
    static const int MIN_FOCUS_MINUTES = 5;
    static const int MAX_FOCUS_MINUTES = 60;
    static const int FOCUS_STEP_MINUTES = 5;

    // ---------- Actions ----------

    // READY only: change the focus length by encoder steps
    void adjustFocus(int steps);

    // READY: start focus, FOCUS_DONE: start break, CYCLE_DONE: new cycle
    void start(uint32_t nowMs, int32_t clockSec);

    void pause(uint32_t nowMs, int32_t clockSec);
    void resume(uint32_t nowMs, int32_t clockSec);

    // PAUSED focus: stop early (the minutes so far still count).
    // PAUSED break, BREAK or FOCUS_DONE: skip the break.
    void stop();

    // Clears today's total, call when the date changes
    void resetToday() { todayFocusSec = 0; }

    // Call often from loop(). Returns true when the state changed by itself.
    bool tick(uint32_t nowMs);

    // ---------- Read by the page ----------

    State state() const { return currentState; }
    State pausedState() const { return stateBeforePause; }
    bool isActive() const { return currentState != READY && currentState != CYCLE_DONE; }

    int focusMinutes() const { return focusLength; }
    int session() const;                 // 1 to SESSIONS, the one shown on screen
    int completedSessions() const { return completed; }

    int minutesLeft() const;             // Rounded up, only minutes are shown
    int phaseMinutes() const;            // Length of the current phase
    int elapsedMinutes() const { return phaseMinutes() - minutesLeft(); }

    int endsAtMinute() const { return endsAt; }        // Minute of the day, -1 = unknown
    int pausedAtMinute() const { return pausedAt; }
    int sessionStartMinute(int index) const { return sessionStart[index]; }

    int todayFocusMinutes(uint32_t nowMs) const;
    int cycleFocusMinutes() const { return cycleFocusSec / 60; }

private:
    State currentState = READY;
    State stateBeforePause = FOCUS;

    int focusLength = 25;   // Minutes
    int completed = 0;      // Sessions finished in this cycle

    uint32_t phaseEndMs = 0;    // While running
    uint32_t remainingMs = 0;   // Updated by tick(), kept while paused
    uint32_t focusStartMs = 0;  // When the current focus run (re)started

    int endsAt = -1;
    int pausedAt = -1;
    int sessionStart[SESSIONS] = {-1, -1, -1, -1};

    int32_t todayFocusSec = 0;
    int32_t cycleFocusSec = 0;

    void startPhase(State phase, uint32_t durationMs, uint32_t nowMs, int32_t clockSec);
    void addFocusTime(uint32_t nowMs);
    void finishFocus(uint32_t nowMs);
};
