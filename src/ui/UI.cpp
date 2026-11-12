#include "UI.h"

#include "Components.h"

#include <Arduino.h>

void UI::begin(EinkDisplay& epd, const AppData& appData) {
    display = &epd;
    data = &appData;
    redrawNeeded = true;
}

// ---------- Input ----------

void UI::noteInput() {
    lastInputMs = millis();
    anyInput = true;
}

bool UI::inUse() const {
    return anyInput && millis() - lastInputMs < INPUT_ACTIVE_MS;
}

void UI::onRotate(int steps) {
    if (steps != 0) {
        noteInput();
    }
    if (steps == 0 || wakeUp()) {
        return;
    }

    if (currentPage == MENU) {
        menuPage.move(steps);  // Change the selected line
        redrawNeeded = true;
    } else if (currentPage == CALENDAR) {
        calendarPage.move(*data, steps);  // Next or previous day
        redrawNeeded = true;
    } else if (currentPage == ELECTRICITY) {
        electricityPage.toggleDay();  // Today <-> tomorrow
    if (currentPage == POMODORO) {
        onPomodoroRotate(steps);
    } else if (currentPage == CALENDAR) {
        // "Turn: mode", Calendar stays selected to come back
        menuPage.select(MenuPage::CALENDAR);
        openPage(MENU);
    } else if (currentPage == HOME) {
        openPage(MENU);
    } else {
        menuPage.move(steps);
        redrawNeeded = true;
    }
}

void UI::onPress() {
    noteInput();
    if (wakeUp()) {
        return;
    }

    if (currentPage == SETUP) {
        setupCancelRequested = data->setupCanCancel;  // First setup can't be skipped
        return;
    }
    if (currentPage == SETTINGS) {
        // Not on Wi-Fi: start the Wi-Fi setup (otherwise everything is on desk.local)
        setupRequested = data->deviceIp[0] == '\0';
        return;
    }

    // Outside the menu, press opens it; in the menu it opens the selected line
    if (currentPage != MENU) {
        openPage(MENU);
        return;
    }
    if (currentPage == POMODORO) {
        onPomodoroPress();
        return;
    }
    if (currentPage == CALENDAR) {
        calendarPage.next();
        redrawNeeded = true;
        return;
    }

    switch (menuPage.selected()) {
        case MenuPage::DASHBOARD:
            openPage(HOME);
            break;
        case MenuPage::CALENDAR:
            calendarPage.open();  // Today
            openPage(CALENDAR);
            break;
        case MenuPage::ELECTRICITY:
            electricityPage.open();  // Today
            openPage(ELECTRICITY);
            break;
        case MenuPage::SETTINGS:
            settingsOpened = true;  // main.cpp fills in the address and QR code
            openPage(SETTINGS);
        case MenuPage::POMODORO:
            openPage(POMODORO);
            break;
        case MenuPage::CALENDAR:
            calendarPage.showToday();
            openPage(CALENDAR);
            break;
        default:
            // TODO: Open the other mode pages when they are implemented
            break;
    }
}

void UI::onLongPress() {
    noteInput();
    if (wakeUp()) {
        return;
    }

    if (currentPage == HOME || currentPage == WIDGETS) {
        // main.cpp fetches new data, then requests a redraw
        refreshRequested = true;
    } else if (currentPage == SETUP) {
        setupRestartRequested = true;
    } else if (currentPage == POMODORO) {
        onPomodoroLongPress();
    } else if (currentPage == CALENDAR) {
        calendarPage.toggleView();  // Day and week view
        redrawNeeded = true;
    } else if (currentPage != HOME) {
        openPage(HOME);
    }
}

void UI::onTouch() {
    noteInput();
    if (wakeUp()) {
        return;
    }

    if (currentPage == SETUP) {
        return;  // Setup stays until it is done or cancelled
    }

    // Switch between the two home pages; from anywhere else, the widget page
    openPage(currentPage == WIDGETS ? HOME : WIDGETS);
// ---------- Pomodoro ----------

void UI::onPomodoroRotate(int steps) {
    if (pomodoro.state() == Pomodoro::READY) {
        pomodoro.adjustFocus(steps);
        redrawNeeded = true;
    } else {
        // Timer keeps running in the background, Pomodoro stays selected to come back
        menuPage.select(MenuPage::POMODORO);
        openPage(MENU);
    }
}

void UI::onPomodoroPress() {
    uint32_t now = millis();

    switch (pomodoro.state()) {
        case Pomodoro::FOCUS:
        case Pomodoro::BREAK:
            pomodoro.pause(now, clockSeconds());
            break;
        case Pomodoro::PAUSED:
            pomodoro.resume(now, clockSeconds());
            break;
        default:  // READY, FOCUS_DONE, CYCLE_DONE
            pomodoro.start(now, clockSeconds());
            break;
    }
    redrawNeeded = true;
}

void UI::onPomodoroLongPress() {
    switch (pomodoro.state()) {
        case Pomodoro::READY:
        case Pomodoro::CYCLE_DONE:
            openPage(HOME);
            break;
        case Pomodoro::FOCUS:
            break;  // Pause first, so a slip of the finger can't end the session
        default:    // PAUSED, BREAK, FOCUS_DONE
            pomodoro.stop();
            redrawNeeded = true;
            break;
    }
}

void UI::updatePomodoro() {
    // Phase ended by itself: show the alert or the next session from any page
    if (pomodoro.tick(millis())) {
        openPage(POMODORO);
    }

    // With a synced clock, main.cpp already redraws every minute. Without it,
    // redraw when the minutes shown change, but never more than that.
    if (currentPage == POMODORO && !data->timeValid &&
        pomodoro.minutesLeft() != pomodoroDrawnMinutes) {
        redrawNeeded = true;
    }

    if (data->timeValid && data->now.tm_yday != lastDay) {
        if (lastDay != -1) {
            pomodoro.resetToday();
        }
        lastDay = data->now.tm_yday;
    }
}

int32_t UI::clockSeconds() const {
    if (!data->timeValid) {
        return -1;
    }
    return data->now.tm_hour * 3600 + data->now.tm_min * 60 + data->now.tm_sec;
}

// ---------- Rendering ----------

void UI::update() {
    if (display == nullptr || data == nullptr) {
        return;
    }

    updatePomodoro();
    if (!redrawNeeded) {
        return;
    }
    if (display->isBusy()) {
        return;  // Draw the newest state once the panel is free
    }

    display->clearBuffer();
    display->fillScreen(COLOR_BG);

    switch (currentPage) {
        case HOME:
            homePage.draw(*display, *data);
            break;
        case WIDGETS:
            widgetPage.draw(*display, *data);
            break;
        case SETTINGS:
            settingsPage.draw(*display, *data);
            break;
        case MENU:
            menuPage.draw(*display, *data);
            break;
        case CALENDAR:
            calendarPage.draw(*display, *data);
            break;
        case ELECTRICITY:
            electricityPage.draw(*display, *data);
            break;
        case SETUP:
            setupPage.draw(*display, *data);
        case POMODORO:
            pomodoroPage.draw(*display, *data, pomodoro, millis());
            pomodoroDrawnMinutes = pomodoro.minutesLeft();
            break;
        case CALENDAR:
            calendarPage.draw(*display, *data);
            break;
        case SLEEP:
            sleepPage.draw(*display, *data);
            break;
    }

    display->startRefresh(inUse());  // No deep clean while the controls are in use
    redrawNeeded = false;
}

bool UI::takeRefreshRequest() {
    bool requested = refreshRequested;
    refreshRequested = false;
    return requested;
}

void UI::enterSleep() {
    if (pomodoro.isActive()) {
        return;  // Keep the timer visible
    }
    if (currentPage != SLEEP) {
        openPage(SLEEP);
    }
}

void UI::openSetup() {
    openPage(SETUP);
}

void UI::closeSetup() {
    if (currentPage == SETUP) {
        openPage(HOME);
    }
}

bool UI::takeSetupRequest() {
    bool requested = setupRequested;
    setupRequested = false;
    return requested;
}

bool UI::takeSettingsOpened() {
    bool opened = settingsOpened;
    settingsOpened = false;
    return opened;
}

bool UI::takeSetupRestart() {
    bool requested = setupRestartRequested;
    setupRestartRequested = false;
    return requested;
}

bool UI::takeSetupCancel() {
    bool requested = setupCancelRequested;
    setupCancelRequested = false;
    return requested;
}

void UI::showNextPage() {
    // Add new pages here so the test mode shows them too
    switch (currentPage) {
        case HOME:
            openPage(WIDGETS);
            break;
        case WIDGETS:
            menuPage.select(MenuPage::DASHBOARD);
            openPage(MENU);
            break;
        case SETTINGS:
            openPage(HOME);
            break;
        case MENU:
            // Walk through the menu one line at a time, then go on
            if (menuPage.selected() == MenuPage::ITEM_COUNT - 1) {
                openPage(POMODORO);
            } else {
                menuPage.move(1);
                redrawNeeded = true;
            }
            break;
        case CALENDAR:
            electricityPage.open();
            openPage(ELECTRICITY);
            break;
        case ELECTRICITY:
            openPage(SLEEP);
            break;
        case SETUP:
            break;  // Setup stays until it is done
        case POMODORO:
            calendarPage.showToday();
            if (calendarPage.isWeekView()) {
                calendarPage.toggleView();
            }
            openPage(CALENDAR);
            break;
        case CALENDAR:
            // Day view first, then the week view
            if (calendarPage.isWeekView()) {
                openPage(SLEEP);
            } else {
                calendarPage.toggleView();
                redrawNeeded = true;
            }
            break;
        case SLEEP:
            openPage(HOME);
            break;
    }
}

bool UI::wakeUp() {
    if (currentPage != SLEEP) {
        return false;
    }
    openPage(HOME);
    return true;
}

void UI::openPage(Page page) {
    currentPage = page;
    redrawNeeded = true;
}
