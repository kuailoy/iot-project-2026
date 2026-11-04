#pragma once

#include "../display/EinkDisplay.h"

#include "pages/CalendarPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/SleepPage.h"

// Keeps track of the current page, handles encoder input
// and redraws the E-Ink display only when something changed.
class UI {
public:
    // Call once after the E-Ink display has been initialized.
    // Pages read everything they show from data.
    void begin(EinkDisplay& display, const AppData& data);

    // Encoder events, call from handleInput()
    void onRotate(int steps);
    void onPress();
    void onLongPress();

    // Touch button: wakes up from sleep, otherwise opens the calendar
    void onTouch();

    // Call from updateDisplay(), does nothing if no redraw is needed.
    // Starts the E-Ink update and returns; while the panel is busy, new input
    // is collected and the newest state is drawn when it is free again.
    void update();

    // True if a control was used in the last INPUT_ACTIVE_MS; slow work
    // (downloads, the deep-clean refresh) waits until then
    bool inUse() const;

    // Force a full redraw, e.g. after new sensor or weather data
    void requestRedraw() { redrawNeeded = true; }

    // True once after the user pressed "refresh" on the Home page
    bool takeRefreshRequest();

    // Show the sleep screen, any input wakes up to Home
    void enterSleep();
    bool isAsleep() const { return currentPage == SLEEP; }

    // Test mode (UI_DEMO_MODE): open the next page, back to Home after the last.
    // On the menu it first selects each line in turn.
    void showNextPage();

private:
    enum Page {
        HOME,
        MENU,
        CALENDAR,
        SLEEP
    };

    EinkDisplay* display = nullptr;
    const AppData* data = nullptr;
    Page currentPage = HOME;
    bool redrawNeeded = true;
    bool refreshRequested = false;

    static constexpr unsigned long INPUT_ACTIVE_MS = 15000;
    unsigned long lastInputMs = 0;
    bool anyInput = false;
    void noteInput();

    HomePage homePage;
    MenuPage menuPage;
    SleepPage sleepPage;
    CalendarPage calendarPage;

    // Returns true if the input was used to wake up
    bool wakeUp();

    void openPage(Page page);
};
