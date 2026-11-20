#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// A widget page: widgets placed on a 4 x 3 grid (see WidgetLayout.h).
// The pages and their layouts are chosen on the phone (settings web page).
// With more than one page in the list, dots at the bottom show which one this
// is among all of them (dedicated pages count too). `page`: which widget page.
class WidgetPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data, int page = 0);
};
