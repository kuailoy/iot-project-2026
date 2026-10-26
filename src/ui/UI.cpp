#include "UI.h"

void UI::begin(Adafruit_EPD& epd, const AppData& appData) {
    display = &epd;
    data = &appData;
    redrawNeeded = true;
}

// ---------- Input ----------

void UI::onRotate(int steps) {
    if (steps == 0 || wakeUp()) {
        return;
    }

    if (currentPage == HOME) {
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

    switch (menuPage.selected()) {
        case MenuPage::DASHBOARD:
            openPage(HOME);
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

    if (currentPage != HOME) {
        openPage(HOME);
    }
}

// ---------- Rendering ----------

void UI::update() {
    if (display == nullptr || data == nullptr || !redrawNeeded) {
        return;
    }

    display->clearBuffer();

    switch (currentPage) {
        case HOME:
            homePage.draw(*display, *data);
            break;
        case MENU:
            menuPage.draw(*display, *data);
            break;
        case SLEEP:
            sleepPage.draw(*display, *data);
            break;
    }

    display->display();
    redrawNeeded = false;
}

bool UI::takeRefreshRequest() {
    bool requested = refreshRequested;
    refreshRequested = false;
    return requested;
}

void UI::enterSleep() {
    if (currentPage != SLEEP) {
        openPage(SLEEP);
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
