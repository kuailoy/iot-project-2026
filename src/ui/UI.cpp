#include "UI.h"

#include "Components.h"

#include <Arduino.h>

void UI::begin(EinkDisplay& epd, const AppData& appData) {
    display = &epd;
    data = &appData;
    redrawNeeded = true;
}

// ---------- Input ----------

void UI::onRotate(int steps) {
    if (steps == 0 || wakeUp()) {
        return;
    }

    if (currentPage == POMODORO) {
        onPomodoroRotate(steps);
    } else if (currentPage == HOME) {
        openPage(MENU);
    } else {
        menuPage.move(steps);
        redrawNeeded = true;
    }
}

void UI::onPress() {
    if (wakeUp()) {
        return;
    }

    if (currentPage == HOME) {
        // main.cpp fetches new data, then requests a redraw
        refreshRequested = true;
        return;
    }
    if (currentPage == POMODORO) {
        onPomodoroPress();
        return;
    }

    switch (menuPage.selected()) {
        case MenuPage::DASHBOARD:
            openPage(HOME);
            break;
        case MenuPage::POMODORO:
            openPage(POMODORO);
            break;
        default:
            // TODO: Open the other mode pages when they are implemented
            break;
    }
}

void UI::onLongPress() {
    if (wakeUp()) {
        return;
    }

    if (currentPage == POMODORO) {
        onPomodoroLongPress();
    } else if (currentPage != HOME) {
        openPage(HOME);
    }
}

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

    display->clearBuffer();
    display->fillScreen(COLOR_BG);

    switch (currentPage) {
        case HOME:
            homePage.draw(*display, *data);
            break;
        case MENU:
            menuPage.draw(*display, *data);
            break;
        case POMODORO:
            pomodoroPage.draw(*display, *data, pomodoro, millis());
            pomodoroDrawnMinutes = pomodoro.minutesLeft();
            break;
        case SLEEP:
            sleepPage.draw(*display, *data);
            break;
    }

    display->refresh();  // Picks full or partial waveform itself
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

void UI::showNextPage() {
    // Add new pages here so the test mode shows them too
    switch (currentPage) {
        case HOME:
            menuPage.select(MenuPage::DASHBOARD);
            openPage(MENU);
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
        case POMODORO:
            openPage(SLEEP);
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
