#include "Habits.h"

const char* const HABIT_NAMES[HABIT_COUNT] = {
    "Drink water", "Read 20 pages", "Finnish practice", "Walk 30 min", "No phone after 22:00",
};

static bool validHabit(int habit) {
    return habit >= 0 && habit < HABIT_COUNT;
}

bool Habits::rollTo(int32_t day) {
    if (today == NO_DAY) {
        today = day;
        return true;
    }
    if (day <= today) {
        return false;  // Same day, or the clock went back: keep the history as it is
    }
    int32_t shift = day - today;
    for (int i = 0; i < HABIT_COUNT; i++) {
        history[i] = shift >= HABIT_HISTORY_DAYS ? 0 : history[i] << shift;
    }
    today = day;
    return true;
}

bool Habits::done(int habit, int daysAgo) const {
    if (!validHabit(habit) || daysAgo < 0 || daysAgo >= HABIT_HISTORY_DAYS) {
        return false;
    }
    return (history[habit] >> daysAgo) & 1u;
}

void Habits::toggleToday(int habit) {
    if (validHabit(habit) && known()) {
        history[habit] ^= 1u;
    }
}

int Habits::doneToday() const {
    int count = 0;
    for (int i = 0; i < HABIT_COUNT; i++) {
        count += done(i, 0) ? 1 : 0;
    }
    return count;
}

int Habits::streak(int habit) const {
    int start = done(habit, 0) ? 0 : 1;
    int days = 0;
    while (start + days < HABIT_HISTORY_DAYS && done(habit, start + days)) {
        days++;
    }
    return days;
}

void Habits::restore(int32_t savedDay, const uint64_t* savedHistory) {
    today = savedDay;
    for (int i = 0; i < HABIT_COUNT; i++) {
        history[i] = savedHistory[i];
    }
}
