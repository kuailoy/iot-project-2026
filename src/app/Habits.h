#pragma once

#include <stdint.h>

// Daily habits for the Habit tracker page: which habit was done on which day,
// for the last HABIT_HISTORY_DAYS days. No Arduino code here, so the UI
// preview can test it on a PC; saving to flash is in HabitStore.h.
// Design: assets/UI-figma-E-ink/20_eink-habit-tracker-1280x800.png

constexpr int HABIT_COUNT = 5;
constexpr int HABIT_HISTORY_DAYS = 64;  // One bit per day; streaks longer than this show as 64d

// The habits, fixed for now (as in the design)
extern const char* const HABIT_NAMES[HABIT_COUNT];

class Habits {
public:
    // Day numbers are DateUtil days (days since 1970-01-01, local time).
    // Moves the history on so that "today" is `day`. Call whenever the date
    // may have changed; returns true if it did.
    bool rollTo(int32_t day);

    // The date is known (rollTo was called at least once, or loaded)
    bool known() const { return today != NO_DAY; }
    int32_t day() const { return today; }

    // Done `daysAgo` days ago (0 = today)
    bool done(int habit, int daysAgo) const;

    // Check off today, or undo it
    void toggleToday(int habit);

    // How many habits are done today
    int doneToday() const;

    // Days in a row the habit was done, up to today. While today isn't done
    // yet, the streak up to yesterday still counts (it can still go on).
    int streak(int habit) const;

    // For saving (HabitStore): bit n = done n days before day()
    uint64_t history[HABIT_COUNT] = {};
    void restore(int32_t savedDay, const uint64_t* savedHistory);

    static constexpr int32_t NO_DAY = -2147483647 - 1;  // Date not known yet

private:
    int32_t today = NO_DAY;
};
