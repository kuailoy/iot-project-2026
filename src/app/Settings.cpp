#include "Settings.h"

#include <Preferences.h>
#include <esp_random.h>

#include "WidgetLayout.h"

// Calendar link from .env.local (tools/load_env.py), used until one is saved
#ifndef CALENDAR_ICS_URL
#define CALENDAR_ICS_URL ""
#endif

namespace {

constexpr const char* NAMESPACE = "settings";

// 8 characters without look-alikes (no 0/o, 1/l/i), easy to type from the screen
String makePassword() {
    static const char CHARS[] = "abcdefghjkmnpqrstuvwxyz23456789";
    String password;
    for (int i = 0; i < 8; i++) {
        password += CHARS[esp_random() % (sizeof(CHARS) - 1)];
    }
    return password;
}

}  // namespace

namespace SettingsStore {

void load(Settings& settings) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);

    settings.wifiSsid = prefs.getString("ssid", "");
    settings.wifiPassword = prefs.getString("pass", "");
    settings.calendarUrl = prefs.isKey("cal") ? prefs.getString("cal", "") : String(CALENDAR_ICS_URL);
    settings.apPassword = prefs.getString("apPass", "");
    settings.widgets = prefs.isKey("w2") ? prefs.getString("w2", "") : String(DEFAULT_WIDGETS);
    settings.cityName = prefs.getString("city", "Kuopio");
    settings.cityLat = prefs.getFloat("lat", 62.89f);
    settings.cityLon = prefs.getFloat("lon", 27.68f);

    // First start: make the setup Wi-Fi password once and keep it
    if (settings.apPassword.length() < 8) {
        settings.apPassword = makePassword();
        prefs.putString("apPass", settings.apPassword);
    }
    prefs.end();
}

void save(const Settings& settings) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putString("ssid", settings.wifiSsid);
    prefs.putString("pass", settings.wifiPassword);
    prefs.putString("cal", settings.calendarUrl);
    prefs.putString("apPass", settings.apPassword);
    prefs.putString("w2", settings.widgets);
    prefs.putString("city", settings.cityName);
    prefs.putFloat("lat", settings.cityLat);
    prefs.putFloat("lon", settings.cityLon);
    prefs.end();
}

}
