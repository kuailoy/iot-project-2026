#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Electricity spot price: the price now, the cheapest 3 hours and a chart
// of the day. Turning the knob switches between today and tomorrow.
// Design: assets/UI-figma-E-ink/23_eink-electricity-1280x800-trmnl-font 1.png
class ElectricityPage {
public:
    // Start on today
    void open() { tomorrow = false; }

    void toggleDay() { tomorrow = !tomorrow; }
    bool showsTomorrow() const { return tomorrow; }

    void draw(Adafruit_GFX& gfx, const AppData& data);

private:
    bool tomorrow = false;
};
