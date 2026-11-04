#include "UI.h"

#include "Components.h"

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

    switch (menuPage.selected()) {
        case MenuPage::DASHBOARD:
            openPage(HOME);
            break;
        case MenuPage::CALENDAR:
            calendarPage.open();  // Today
            openPage(CALENDAR);
            break;
        case MenuPage::SETTINGS:
            settingsOpened = true;  // main.cpp fills in the address and QR code
            openPage(SETTINGS);
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
    } else {
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
}

// ---------- Rendering ----------

void UI::update() {
    if (display == nullptr || data == nullptr || !redrawNeeded) {
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
        case SETUP:
            setupPage.draw(*display, *data);
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
    if (currentPage != SLEEP && currentPage != SETUP) {
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
                calendarPage.open();
                openPage(CALENDAR);
            } else {
                menuPage.move(1);
                redrawNeeded = true;
            }
            break;
        case CALENDAR:
            openPage(SLEEP);
            break;
        case SETUP:
            break;  // Setup stays until it is done
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
