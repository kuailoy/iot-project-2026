// Knob and drawing for the Habit tracker page.
// Part of the UI class, kept apart from UI.cpp; UI.cpp calls these first.

#include "UI.h"

#include "Components.h"
#include "../app/DateUtil.h"
#include "../app/HabitStore.h"

bool UI::rotateHabits(int steps) {
    (void)steps;
    if (currentPage != HABITS) {
        return false;
    }
    // "Turn: mode", Habits stays selected to come back
    menuPage.select(MenuPage::HABITS);
    openPage(MENU);
    return true;
}

bool UI::pressHabits() {
    if (currentPage != HABITS) {
        return false;
    }
    // "Press: check off" (again: undo); needs the date
    if (habits.known()) {
        habits.toggleToday(habitPage.selected());
        HabitStore::save(habits);
        redrawNeeded = true;
    }
    return true;
}

bool UI::longPressHabits() {
    if (currentPage != HABITS) {
        return false;
    }
    habitPage.next();  // "Hold: next habit"
    redrawNeeded = true;
    return true;
}

void UI::updateHabits() {
    // A new day moves the habit history on
    if (data->timeValid && habits.rollTo(DateUtil::dayOf(data->now)) && currentPage == HABITS) {
        redrawNeeded = true;
    }
}

bool UI::drawHabits() {
    if (currentPage == MENU) {
        menuPage.setHabits(habits.doneToday(), HABIT_COUNT, habits.known());
        return false;  // UI.cpp draws the menu
    }
    if (currentPage != HABITS) {
        return false;
    }
    habitPage.draw(*display, *data, habits);
    return true;
}
