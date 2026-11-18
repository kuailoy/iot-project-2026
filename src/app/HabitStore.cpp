#include "HabitStore.h"

#include <Preferences.h>

namespace {

constexpr const char* NAMESPACE = "habits";

}  // namespace

namespace HabitStore {

void load(Habits& habits) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) {
        return;  // Nothing saved yet (first start or after a reset)
    }
    uint64_t history[HABIT_COUNT] = {};
    bool complete = prefs.isKey("day") && prefs.getBytesLength("bits") == sizeof(history);
    if (complete) {
        prefs.getBytes("bits", history, sizeof(history));
        habits.restore(prefs.getInt("day", Habits::NO_DAY), history);
    }
    prefs.end();
}

void save(const Habits& habits) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putInt("day", habits.day());
    prefs.putBytes("bits", habits.history, sizeof(habits.history));
    prefs.end();
}

}
