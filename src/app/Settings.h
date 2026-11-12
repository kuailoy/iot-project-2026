#pragma once

#include <Arduino.h>

// Settings kept in flash (NVS), so they survive restarts and re-flashing.
// Changed on the setup web page (SetupPortal).
struct Settings {
    String wifiSsid;       // Home Wi-Fi, "" = not set up yet: show the setup page
    String wifiPassword;
    String calendarUrl;    // iCal / webcal link, "" = no calendar
    String apPassword;     // Password of the setup Wi-Fi, made once on first start
    String widgets;        // Second home page layout, e.g. "clock:W,weather:M" (WidgetLayout.h)
    String cityName;       // Weather location, shown on the screen
    float cityLat = 0;
    float cityLon = 0;

    bool hasWifi() const { return wifiSsid.length() > 0; }
};

namespace SettingsStore {

// Load everything. The calendar link defaults to CALENDAR_ICS_URL from
// .env.local until one is saved on the settings page; the city to Kuopio.
void load(Settings& settings);

void save(const Settings& settings);

}
