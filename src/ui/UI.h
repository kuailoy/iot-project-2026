#pragma once

#include <Adafruit_EPD.h>

#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/SleepPage.h"

// Keeps track of the current page, handles encoder input
// and redraws the E-Ink display only when something changed.
class UI {
public:
    // Call once after the E-Ink display has been initialized.
    // Pages read everything they show from data.
    void begin(Adafruit_EPD& display, const AppData& data);

    // Encoder events, call from handleInput()
    void onRotate(int steps);
    void onPress();
    void onLongPress();

    // Call from updateDisplay(), does nothing if no redraw is needed
    void update();

    // Force a full redraw, e.g. after new sensor or weather data
    void requestRedraw() { redrawNeeded = true; }

    // True once after the user pressed "refresh" on the Home page
    bool takeRefreshRequest();

    // Show the sleep screen, any input wakes up to Home
    void enterSleep();
    bool isAsleep() const { return currentPage == SLEEP; }

private:
    enum Page {
        HOME,
        MENU,
        SLEEP
    };

    Adafruit_EPD* display = nullptr;
    const AppData* data = nullptr;
    Page currentPage = HOME;
    bool redrawNeeded = true;
    bool refreshRequested = false;

    HomePage homePage;
    MenuPage menuPage;
    SleepPage sleepPage;

    // Returns true if the input was used to wake up
    bool wakeUp();

    void openPage(Page page);
};
