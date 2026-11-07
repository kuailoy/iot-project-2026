#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "../app/Settings.h"
#include "ClaudeUsage.h"

// Web pages of the device.
//
// First setup ("captive portal", only while no home Wi-Fi works):
// 1. The device opens its own Wi-Fi, "Desk-Setup-XXXX", with a password.
//    The screen shows a QR code with both, so a phone camera joins it.
// 2. A DNS server sends every name to the device, so the phone thinks it is
//    a hotel-style login network and opens the setup page by itself.
//    It is also reachable at http://192.168.4.1.
// 3. On the page the user picks the home Wi-Fi and types its password.
//    The device tries the network; if it works it is saved, otherwise the
//    page says so and setup goes on.
//
// Everything runs from loop(), so the screen and the knob keep working.
//
// All other settings are on the home Wi-Fi at http://desk.local (or the
// device's IP), see startLan(): widgets of the second home page, weather
// city, calendar link, and changing the Wi-Fi (falls back to the old one).
class SetupPortal {
public:
    enum class State : uint8_t {
        OFF,           // Not running
        WAITING,       // Setup Wi-Fi is on, no phone yet
        PHONE_JOINED,  // A phone joined the setup Wi-Fi
        CONNECTING,    // Trying the home Wi-Fi from the page
        CONNECTED,     // It works, settings saved
        FAILED         // It didn't work, the user can try again
    };

    // Start the setup Wi-Fi and the page. `settings` is updated and saved
    // once the chosen home Wi-Fi works.
    void begin(Settings& settings);

    // Stop the setup Wi-Fi. The home Wi-Fi stays connected if it is.
    void end();

    // Serve the settings pages on the home Wi-Fi (http://desk.local).
    // Call once connected; does nothing while the setup Wi-Fi is on.
    void startLan(Settings& settings);
    bool lanRunning() const { return lanOn; }

    // OpenWeatherMap key for the city search
    void setWeatherKey(const char* key) { weatherKey = key; }

    // True once after these were changed on the phone (already saved)
    bool takeWidgetsChanged();
    bool takeCityChanged();
    bool takeCalendarChanged();
    bool takeWifiChanged();      // The home Wi-Fi was switched on the settings page

    // Claude plan usage sent by the browser extension (POST /claude).
    // True once per new message, with the numbers in `usage`.
    bool takeClaudeUsage(ClaudeUsage& usage);

    // Call often from loop()
    void loop();

    bool active() const { return state != State::OFF; }

    // True once after something the screen shows has changed
    bool takeChanged();

    // Connected, and the phone had a few seconds to show it: call end()
    bool finished() const;

    const String& networkName() const { return apName; }

    // One line for the screen, e.g. "Connecting to Home-WiFi..."
    String statusText() const;

private:
    struct Network {
        String ssid;
        int rssi;
        bool open;
    };
    static constexpr int MAX_NETWORKS = 20;

    DNSServer dns;
    WebServer server{80};
    bool routesAdded = false;
    bool lanOn = false;
    const char* weatherKey = "";
    bool widgetsChanged = false;
    bool cityChanged = false;
    bool calendarChanged = false;
    bool wifiChanged = false;
    ClaudeUsage claudeUsage;
    bool claudeReceived = false;
    unsigned long claudeAtMs = 0;   // When the last numbers came, 0 = never

    // Switching the home Wi-Fi from the settings page
    enum class Switch : uint8_t { IDLE, PENDING, SWITCHING, DONE, FAILED };
    Switch switchState = Switch::IDLE;
    String switchSsid;
    String switchPassword;
    String oldSsid;
    String oldPassword;
    unsigned long switchStart = 0;
    bool mdnsRestart = false;
    void loopSwitch();
    void restartMdns();
    Settings* settings = nullptr;

    State state = State::OFF;
    bool changed = false;
    String apName;

    Network networks[MAX_NETWORKS];
    int networkCount = 0;

    String pendingSsid;
    String pendingPassword;
    unsigned long connectStart = 0;
    unsigned long connectedAt = 0;
    String failReason;

    void setState(State next);
    void scan();

    void addRoutes();
    void handleRoot();
    void handleWidgets();
    void handleWidgetsSave();
    void handleSettings();
    void handleSettingsSave();
    void handleCity();
    void handleWifiPage();
    void handleWifiSave();
    void handleWifiStatus();
    void handleClaude();
    void handleClaudePreflight();
    // Network list and password fields, shared by setup and the Wi-Fi page
    String wifiFields(const String& preselect, const char* refreshLink);
    // ssid / password from a submitted form; false (and a page sent) if invalid
    bool readWifiForm(String& ssid, String& password);
    void handleSave();
    void handleStatus();
    void handleScan();
    void redirectToRoot();
    void sendPage(const String& body, int refreshSeconds = 0);
    // icon = nullptr shows a spinner
    void sendStatusPage(const char* icon, const String& title, const String& text, const String& button,
                        int refreshSeconds = 0);
};

// QR code that joins a Wi-Fi network ("WIFI:T:WPA;S:name;P:password;;"),
// understood by the iPhone and Android cameras. `bits` gets one bit per
// module, row by row, MSB first. Returns the modules per side, 0 on failure.
int makeWifiQrCode(const String& ssid, const String& password, uint8_t* bits, size_t bitsSize);

// Same for any text, e.g. a link
int makeQrCode(const String& text, uint8_t* bits, size_t bitsSize);
