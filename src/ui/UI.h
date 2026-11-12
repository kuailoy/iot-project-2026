#pragma once

#include "../display/EinkDisplay.h"

#include "pages/CalendarPage.h"
#include "pages/ElectricityPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/MusicPage.h"
#include "pages/SettingsPage.h"
#include "pages/SetupPage.h"
#include "pages/PomodoroPage.h"
#include "pages/PowerOffPage.h"
#include "pages/SleepPage.h"
#include "pages/WidgetPage.h"

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

    // Touch button, call from handleInput().
    // Tap (on release): wakes up from sleep, otherwise switches between the
    // two home pages (Home and the widget page).
    void onTouch();
    // Held for a few seconds: on a home page, shows the "turned off" page and
    // asks main.cpp to power off (see readyToPowerOff); elsewhere like a tap.
    void onTouchHold();
    // A touch has started: counts as input, so slow downloads wait (inUse)
    void onTouchDown() { noteInput(); }

    // True once the "turned off" page is fully on the screen: main.cpp
    // can stop everything now, E-Ink keeps the image without power
    bool readyToPowerOff() const;

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

    // Wi-Fi setup page (main.cpp runs the SetupPortal behind it)
    void openSetup();
    void closeSetup();                   // Back to Home
    bool takeSetupRequest();             // Press on the Settings page
    bool takeSettingsOpened();           // Settings page opened: main.cpp fills in the address
    bool takeSetupRestart();             // Hold on the setup page
    bool takeSetupCancel();              // Press on the setup page (only if Wi-Fi was set up before)

    // Show the sleep screen, any input wakes up to Home.
    // Ignored during setup, while turning off and while a Pomodoro timer is running.
    void enterSleep();
    bool isAsleep() const { return currentPage == SLEEP; }

    // Test mode (UI_DEMO_MODE): open the next page, back to Home after the last.
    // On the menu it first selects each line in turn.
    void showNextPage();

private:
    enum Page {
        HOME,
        WIDGETS,   // Second home page
        MENU,
        CALENDAR,
        ELECTRICITY,
        MUSIC,
        SETTINGS,
        SETUP,
        POMODORO,
        SLEEP,
        OFF        // "Turned off", input is ignored until the device is off
    };

    EinkDisplay* display = nullptr;
    const AppData* data = nullptr;
    Page currentPage = HOME;
    bool redrawNeeded = true;
    bool refreshRequested = false;
    bool setupRequested = false;
    bool settingsOpened = false;
    bool setupRestartRequested = false;
    bool setupCancelRequested = false;

    static constexpr unsigned long INPUT_ACTIVE_MS = 15000;
    unsigned long lastInputMs = 0;
    bool anyInput = false;
    void noteInput();

    HomePage homePage;
    MenuPage menuPage;
    SleepPage sleepPage;
    CalendarPage calendarPage;
    ElectricityPage electricityPage;
    MusicPage musicPage;
    SetupPage setupPage;
    SettingsPage settingsPage;
    WidgetPage widgetPage;
    PomodoroPage pomodoroPage;
    PowerOffPage powerOffPage;

    // Turning off (onTouchHold)
    bool powerOffRequested = false;
    bool offPageDrawn = false;      // The refresh with the OFF page has started
    bool offTimeKnown = false;
    struct tm offTime = {};         // Shown on the page, fixed when turning off

    Pomodoro pomodoro;
    int pomodoroDrawnMinutes = -1;
    int lastDay = -1;

    void onPomodoroRotate(int steps);
    void onPomodoroPress();
    void onPomodoroLongPress();
    void updatePomodoro();

    // Seconds since midnight, -1 while the clock is not synced
    int32_t clockSeconds() const;

    // Returns true if the input was used to wake up
    bool wakeUp();

    void openPage(Page page);
};
