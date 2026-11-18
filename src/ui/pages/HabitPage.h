#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"
#include "../../app/Habits.h"

// Habit tracker: the last 7 days of every habit, today in a box, and the
// streak. The knob checks off today for the selected habit (UI.cpp).
// Design: assets/UI-figma-E-ink/20_eink-habit-tracker-1280x800.png
class HabitPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data, const Habits& habits);

    // Selected habit (the arrow): Hold moves it on, wraps around
    void next() { selectedHabit = (selectedHabit + 1) % HABIT_COUNT; }
    int selected() const { return selectedHabit; }

private:
    int selectedHabit = 0;
};
