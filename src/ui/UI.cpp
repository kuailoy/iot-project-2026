#include "UI.h"

void UI::begin(Adafruit_EPD& epd, const AppData& appData) {
    display = &epd;
    data = &appData;
    redrawNeeded = true;
}

// ---------- Input ----------

void UI::onRotate(int steps) {
    if (steps == 0) {
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
    }

    display->display();
    redrawNeeded = false;
}

bool UI::takeRefreshRequest() {
    bool requested = refreshRequested;
    refreshRequested = false;
    return requested;
}

void UI::openPage(Page page) {
    currentPage = page;
    redrawNeeded = true;
}
