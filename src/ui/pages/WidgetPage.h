#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Second home page: widgets placed on a 4 x 3 grid (see WidgetLayout.h).
// The layout is chosen on the phone (settings web page).
class WidgetPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
