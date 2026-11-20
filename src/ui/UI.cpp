#include "UI.h"

#include "Components.h"
#include "../app/HabitStore.h"
#include "../app/HomePages.h"

#include <Arduino.h>

void UI::begin(EinkDisplay& epd, const AppData& appData) {
    display = &epd;
    data = &appData;
    redrawNeeded = true;
    HabitStore::load(habits);
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
    if (steps == 0 || currentPage == OFF || wakeUp()) {
        return;
    }
    if (rotateHabits(steps) || rotateDevice(steps)) {
        return;  // Habit tracker (UIHabits.cpp), Device pages (UIDevice.cpp)
    }

    if (currentPage == MENU) {
        menuPage.move(steps);  // Change the selected line
        redrawNeeded = true;
    } else if (currentPage == CALENDAR) {
        calendarPage.move(*data, steps);  // Next or previous day (week in the week view)
        redrawNeeded = true;
    } else if (currentPage == ELECTRICITY) {
        electricityPage.toggleDay();  // Today <-> tomorrow
        redrawNeeded = true;
    } else if (currentPage == POMODORO) {
        onPomodoroRotate(steps);
    } else if (currentPage == WIDGETS) {
        // Next or previous widget page, stops at the first and the last
        int index = widgetIndex + steps;
        index = index < 0 ? 0 : index >= widgetPages() ? widgetPages() - 1 : index;
        if (index != widgetIndex) {
            openWidgets(index);
        }
    } else if (currentPage == HOME) {
        openPage(MENU);
    }
}

void UI::onPress() {
    noteInput();
    if (currentPage == OFF || wakeUp()) {
        return;
    }
    if (pressHabits() || pressDevice()) {
        return;  // Habit tracker (UIHabits.cpp), Device pages (UIDevice.cpp)
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

    if (currentPage == POMODORO) {
        onPomodoroPress();
        return;
    }
    if (currentPage == CALENDAR) {
        calendarPage.toggleView();  // Day and week view
        redrawNeeded = true;
        return;
    }

    // Elsewhere, press opens the menu; in the menu it opens the selected line
    if (currentPage != MENU) {
        openPage(MENU);
        return;
    }

    switch (menuPage.selected()) {
        case MenuPage::DASHBOARD:
            openStart();
            break;
        case MenuPage::POMODORO:
            openPage(POMODORO);
            break;
        case MenuPage::CALENDAR:
            calendarPage.open();  // Today, day view
            openPage(CALENDAR);
            break;
        case MenuPage::ELECTRICITY:
            electricityPage.open();  // Today
            openPage(ELECTRICITY);
            break;
        case MenuPage::MUSIC:
            openPage(MUSIC);
            break;
        case MenuPage::SETTINGS:
            settingsOpened = true;  // main.cpp fills in the address and QR code
            openPage(SETTINGS);
            break;
        case MenuPage::HABITS:
            openPage(HABITS);
            break;
        case MenuPage::DEVICE:
            devicePage.select(DevicePage::WIFI);
            openPage(DEVICE);
            break;
        default:
            // TODO: Open the other mode pages when they are implemented
            break;
    }
}

void UI::onLongPress() {
    noteInput();
    if (currentPage == OFF || wakeUp()) {
        return;
    }
    if (longPressHabits() || longPressDevice()) {
        return;  // Habit tracker (UIHabits.cpp), Device pages (UIDevice.cpp)
    }

    if (currentPage == HOME || currentPage == WIDGETS) {
        // main.cpp fetches new data, then requests a redraw
        refreshRequested = true;
    } else if (currentPage == SETUP) {
        setupRestartRequested = true;
    } else if (currentPage == POMODORO) {
        onPomodoroLongPress();
    } else {
        openStart();
    }
}

void UI::onTouch() {
    noteInput();
    if (currentPage == OFF || wakeUp()) {
        return;
    }

    if (currentPage == SETUP || currentPage == RESTARTING) {
        return;  // Setup stays until it is done or cancelled; restarting can't be stopped
    }

    // The pages in the order of the list on the phone (HomePages.h); from a
    // page that isn't in the list (menu, settings...), the first one
    int current = listPosition();
    int target = HomePages::next(current, data->pageCount);
    if (target == current) {
        return;  // The only page: nothing to switch to, no refresh
    }
    openListPage(target);
}

void UI::onTouchHold() {
    if (listPosition() < 0) {
        onTouch();  // Power off only from a page of the list
        return;
    }
    noteInput();

    offTimeKnown = data->timeValid;
    offTime = data->now;
    powerOffRequested = true;
    openPage(OFF);
}

bool UI::readyToPowerOff() const {
    return powerOffRequested && currentPage == OFF && offPageDrawn && display != nullptr && !display->isBusy();
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
            openStart();
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
    // (but not over the "turned off" page)
    if (pomodoro.tick(millis()) && currentPage != OFF) {
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
    updateHabits();
    updateDevice();
    if (currentPage == OFF && offPageDrawn) {
        redrawNeeded = false;  // Nothing on it changes (the time is fixed): no second refresh
    }
    if (!redrawNeeded) {
        return;
    }
    if (display->isBusy()) {
        return;  // Draw the newest state once the panel is free
    }

    // The widget page shown was deleted on the phone
    if (currentPage == WIDGETS && widgetIndex >= widgetPages()) {
        if (widgetPages() > 0) {
            widgetIndex = 0;
        } else {
            openStart();
        }
    }

    display->clearBuffer();
    display->fillScreen(COLOR_BG);

    // Habit tracker (also fills in the menu's Habits row) and Device pages
    if (drawHabits() || drawDevice()) {
        display->startRefresh(inUse());
        redrawNeeded = false;
        return;
    }

    switch (currentPage) {
        case HOME:
            homePage.draw(*display, *data);
            break;
        case WIDGETS:
            widgetPage.draw(*display, *data, widgetIndex);
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
        case MUSIC:
            musicPage.draw(*display, *data);
            break;
        case SETUP:
            setupPage.draw(*display, *data);
            break;
        case POMODORO:
            pomodoroPage.draw(*display, *data, pomodoro, millis());
            pomodoroDrawnMinutes = pomodoro.minutesLeft();
            break;
        case SLEEP:
            sleepPage.draw(*display, *data);
            break;
        case OFF:
            powerOffPage.draw(*display, offTimeKnown ? &offTime : nullptr);
            offPageDrawn = true;
            break;
        default:
            break;  // Drawn by drawHabits() / drawDevice()
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
    if (currentPage == SETUP || currentPage == OFF || currentPage == RESTARTING || pomodoro.isActive()) {
        return;  // Keep the QR code, the "turned off" page or the timer visible
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
        openStart();
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
            if (widgetPages() > 0) {
                openWidgets(0);
            } else {
                menuPage.select(MenuPage::DASHBOARD);
                openPage(MENU);
            }
            break;
        case WIDGETS:
            if (widgetIndex + 1 < widgetPages()) {
                openWidgets(widgetIndex + 1);  // Every widget page
            } else {
                menuPage.select(MenuPage::DASHBOARD);
                openPage(MENU);
            }
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
        case POMODORO:
            calendarPage.open();
            openPage(CALENDAR);
            break;
        case CALENDAR:
            // Day view first, then the week view
            if (calendarPage.isWeekView()) {
                electricityPage.open();
                openPage(ELECTRICITY);
            } else {
                calendarPage.toggleView();
                redrawNeeded = true;
            }
            break;
        case ELECTRICITY:
            openPage(MUSIC);
            break;
        case MUSIC:
            openPage(HABITS);
            break;
        case HABITS:
            devicePage.select(DevicePage::WIFI);
            openPage(DEVICE);
            break;
        case DEVICE:
            openPage(RESTART_CONFIRM);
            break;
        case RESTART_CONFIRM:
            openPage(RESET_CONFIRM);
            break;
        case RESET_CONFIRM:
            openPage(RESTARTING);  // Only shown: restartPending stays false, nothing restarts
            break;
        case RESTARTING:
            openPage(SLEEP);
            break;
        case SETUP:
            break;  // Setup stays until it is done
        case SLEEP:
            // Only shown here: powerOffRequested stays false, so nothing turns off
            offTimeKnown = data->timeValid;
            offTime = data->now;
            openPage(OFF);
            break;
        case OFF:
            openPage(HOME);
            break;
    }
}

bool UI::wakeUp() {
    if (currentPage != SLEEP) {
        return false;
    }
    openStart();
    return true;
}

void UI::openPage(Page page) {
    currentPage = page;
    redrawNeeded = true;
    offPageDrawn = false;
    restartingDrawn = false;
    if (page == RESET_CONFIRM) {
        resetPageOpenedMs = millis();  // Only a hold that starts here counts
        resetBoxesDrawn = 0;
    }
}

void UI::openWidgets(int index) {
    if (index >= widgetPages()) {
        openStart();
        return;
    }
    widgetIndex = index;
    openPage(WIDGETS);
}

void UI::openStart() {
    openListPage(0);
}

int UI::listPosition() const {
    for (int i = 0; i < data->pageCount; i++) {
        const PageEntry& entry = data->pageList[i];
        bool here = (entry.kind == PAGE_HOME && currentPage == HOME) ||
                    (entry.kind == PAGE_CALENDAR && currentPage == CALENDAR) ||
                    (entry.kind == PAGE_PRICES && currentPage == ELECTRICITY) ||
                    (entry.kind == PAGE_MUSIC && currentPage == MUSIC) ||
                    (entry.kind == PAGE_POMODORO && currentPage == POMODORO) ||
                    (entry.kind == PAGE_WIDGETS && currentPage == WIDGETS && entry.grid == widgetIndex);
        if (here) {
            return i;
        }
    }
    return -1;
}

void UI::openListPage(int position) {
    if (position < 0 || position >= data->pageCount) {
        position = 0;
    }
    const PageEntry& entry = data->pageList[position];
    switch (entry.kind) {
        case PAGE_HOME:
            openPage(HOME);
            break;
        case PAGE_CALENDAR:
            calendarPage.open();  // Today, day view
            openPage(CALENDAR);
            break;
        case PAGE_PRICES:
            electricityPage.open();  // Today
            openPage(ELECTRICITY);
            break;
        case PAGE_MUSIC:
            openPage(MUSIC);
            break;
        case PAGE_POMODORO:
            openPage(POMODORO);
            break;
        default:
            widgetIndex = entry.grid;
            openPage(WIDGETS);
            break;
    }
    Serial.printf("Screen: page %d of %d, %s\n", position + 1, data->pageCount,
                  entry.kind == PAGE_WIDGETS ? "widgets" : pageName(entry.kind));
}
