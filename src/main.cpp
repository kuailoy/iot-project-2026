
#include <Arduino.h>

// Core: communication and networking
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <memory>
#include <new>

// Display: E-Ink
#include <Adafruit_GFX.h>
#include <Adafruit_EPD.h>
#include <Adafruit_ThinkInk.h>

// Sensor: CO2, temperature and humidity
#include <SensirionI2CScd4x.h>

// Input: rotary encoder
#include <Adafruit_seesaw.h>

// Data: JSON parsing
#include <ArduinoJson.h>

// Project: wiring, shared data and UI
#include "pins.h"
#include "app/AppData.h"
#include "services/WeatherService.h"
#include "services/CalendarService.h"
#include "app/DateUtil.h"
#include "app/Settings.h"
#include "services/SetupPortal.h"
#include "ui/UI.h"
#include "display/EinkDisplay.h"
#include "display/EinkBenchmark.h"

// API keys are injected from .env.local by the PlatformIO pre-script.
// Wi-Fi and the calendar link are set on the setup page (Settings, SetupPortal).
#ifndef WEATHER_API_KEY
#define WEATHER_API_KEY ""
#endif

// ---------- Settings ----------

const char* NTP_SERVER = "pool.ntp.org";
const char* TIME_ZONE = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // Finland, with summer time

// The weather city is chosen on the settings page (desk.local)

const unsigned long WEATHER_INTERVAL_MS = 15 * 60 * 1000UL;
const unsigned long WEATHER_RETRY_MS = 60 * 1000UL;
const unsigned long CALENDAR_INTERVAL_MS = 30 * 60 * 1000UL;
const unsigned long CALENDAR_RETRY_MS = 2 * 60 * 1000UL;

const unsigned long SENSOR_INTERVAL_MS = 30 * 1000UL;
const unsigned long LONG_PRESS_MS = 800;
const unsigned long SLEEP_AFTER_MS = 5 * 60 * 1000UL;  // No input for 5 min

// Wait for time, weather and sensor before the first (slow) E-Ink refresh
const unsigned long FIRST_DRAW_DELAY_MS = 10 * 1000UL;

// Test mode: set UI_DEMO_MODE=1 in platformio.ini to show every page in turn
#ifndef UI_DEMO_MODE
#define UI_DEMO_MODE 0
#endif
const unsigned long DEMO_PAGE_MS = 20 * 1000UL;  // How long each page stays on screen

// ---------- Global objects ----------

// Refresh mode and speed options: EPD_* flags in platformio.ini
EinkDisplay display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
SensirionI2cScd4x scd4x;
Adafruit_seesaw rotary;
AppData appData;
UI ui;
Settings settings;      // Home Wi-Fi and calendar link, saved in flash
SetupPortal portal;     // Wi-Fi setup page, runs when needed
bool downloadNow = false;  // Fetch weather and calendar on the next loop

// Set by the touch interrupt, so a tap during a slow E-Ink refresh is not lost
volatile bool touchPending = false;

bool sensorFound = false;
bool encoderFound = false;

// ---------- Function declarations ----------

void initSerial();
void initI2C();
void initDisplay();
void initSensor();
void initEncoder();
void initTouch();
void initWiFi();
void initTime();

void updateTime();
void readSensor();
void updateDisplay();
void handleInput();
void updateWeather(bool force);
void updateCalendar(bool force);
void showPagesInTurn();
void connectWiFi(bool wait);
void startSetup();
void updateSetup();
void applyWidgets();
void applySettingsChanges();
void applyClaudeUsage(const ClaudeUsage& usage);
void fillSettingsPage();

// ---------- Setup ----------

void setup() {
    initSerial();
    initI2C();

    Serial.println("System starting...");

    initDisplay();

#if EPD_BENCHMARK
    // Benchmark only: the normal app does not start
    runEinkBenchmark(display);
    while (true) {
        delay(1000);
    }
#endif

    initSensor();
    initEncoder();
    initTouch();
    initWiFi();
    initTime();
}

// ---------- Main loop ----------

void loop() {
#if UI_DEMO_MODE
    showPagesInTurn();
#else
    handleInput();
#endif
    updateSetup();
    updateTime();
    readSensor();
    bool refresh = ui.takeRefreshRequest() || downloadNow;
    downloadNow = false;
    updateWeather(refresh);
    updateCalendar(refresh);
    updateDisplay();

    delay(10);
}

// ---------- Initialization ----------

void initSerial() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\nESP32 E-Ink Project");
}

void initI2C() {
    Wire.begin();
    Serial.println("I2C initialized");
}

void initDisplay() {
    if (!display.begin()) {
        Serial.println("Display setup failed");
    }
    ui.begin(display, appData);
    Serial.println("Display initialized");
}

void initSensor() {
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);

    // Sensor may still be measuring after a soft reset, stop it first
    scd4x.stopPeriodicMeasurement();
    delay(500);

    sensorFound = (scd4x.startPeriodicMeasurement() == 0);
    Serial.println(sensorFound ? "SCD-41 found" : "SCD-41 not found");
}

void initEncoder() {
    encoderFound = rotary.begin(ENCODER_I2C_ADDR);
    if (!encoderFound) {
        Serial.println("Rotary encoder not found");
        return;
    }

    rotary.pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    rotary.getEncoderDelta();  // Clear any movement since power-on
    Serial.println("Rotary encoder found");
}

void IRAM_ATTR onTouchInterrupt() {
    touchPending = true;
}

void initTouch() {
    // Pull-down keeps the pin LOW if the module is not connected
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), onTouchInterrupt, RISING);
    Serial.println("Touch button ready");
}

void initWiFi() {
    SettingsStore::load(settings);
    applyWidgets();
    strlcpy(appData.city, settings.cityName.c_str(), sizeof(appData.city));
    portal.setWeatherKey(WEATHER_API_KEY);

    // First start (or after a reset): no home Wi-Fi yet, show the setup page
    if (!settings.hasWifi()) {
        Serial.println("Wi-Fi: not set up yet");
        startSetup();
        return;
    }
    connectWiFi(true);
}

// Connect to the saved home Wi-Fi. wait = block up to 15 s (at start-up)
void connectWiFi(bool wait) {
    Serial.printf("Connecting to %s", settings.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    if (!wait) {
        Serial.println();
        return;  // updateTime() notices when it is connected
    }

    // Give up after 15 s so the device still starts without Wi-Fi
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    appData.wifiConnected = (WiFi.status() == WL_CONNECTED);
    Serial.println(appData.wifiConnected ? " connected" : " failed");
}

// Turn on the setup Wi-Fi and show the setup page with its QR code
void startSetup() {
    portal.begin(settings);

    appData.setupActive = true;
    appData.setupCanCancel = settings.hasWifi();
    appData.wifiConnected = false;
    strlcpy(appData.setupNetwork, portal.networkName().c_str(), sizeof(appData.setupNetwork));
    strlcpy(appData.setupPassword, settings.apPassword.c_str(), sizeof(appData.setupPassword));
    strlcpy(appData.setupStatus, portal.statusText().c_str(), sizeof(appData.setupStatus));
    appData.qrSize =
        makeWifiQrCode(portal.networkName(), settings.apPassword, appData.qrCode, sizeof(appData.qrCode));
    if (appData.qrSize == 0) {
        Serial.println("Setup: QR code could not be made");
    }
    ui.openSetup();
}

// Place the saved widgets of the second home page
void applyWidgets() {
    appData.widgetCount = layoutWidgets(settings.widgets.c_str(), appData.widgets, MAX_WIDGETS);
    ui.requestRedraw();
}

// Address and QR code of the settings web page, for the Settings screen
void fillSettingsPage() {
    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        strlcpy(appData.deviceIp, ip.c_str(), sizeof(appData.deviceIp));
        strlcpy(appData.wifiName, settings.wifiSsid.c_str(), sizeof(appData.wifiName));
        appData.qrSize = makeQrCode("http://" + ip + "/", appData.qrCode, sizeof(appData.qrCode));
    } else {
        appData.deviceIp[0] = '\0';
        appData.qrSize = 0;
    }
    ui.requestRedraw();
}

// New Claude plan numbers from the browser extension. The screen is only
// redrawn when something it shows changes (E-Ink refreshes are slow).
void applyClaudeUsage(const ClaudeUsage& usage) {
    auto percent = [](float value) -> uint8_t {
        return uint8_t(value < 0 ? 0 : value > 100 ? 100 : lroundf(value));
    };
    uint8_t session = percent(usage.sessionPercent);
    uint8_t weekly = percent(usage.weeklyPercent);
    bool changed = !appData.claudeValid || session != appData.claudeSession || weekly != appData.claudeWeekly ||
                   usage.sessionResets != appData.claudeSessionReset ||
                   usage.weeklyResets != appData.claudeWeeklyReset;

    // An "x min ago" note appears after 30 min without news: redraw when it goes away
    bool wasOld = appData.claudeValid && appData.epoch - appData.claudeUpdated > 30 * 60;

    appData.claudeValid = true;
    appData.claudeSession = session;
    appData.claudeWeekly = weekly;
    appData.claudeSessionReset = usage.sessionResets;
    appData.claudeWeeklyReset = usage.weeklyResets;
    appData.claudeUpdated = time(nullptr);
    Serial.printf("Claude usage: session %u%%, week %u%%\n", session, weekly);

    if (changed || wasOld) {
        ui.requestRedraw();
    }
}

// Apply what was changed on the settings page (already saved)
void applySettingsChanges() {
    if (portal.takeWidgetsChanged()) {
        applyWidgets();
    }
    if (portal.takeCityChanged()) {
        strlcpy(appData.city, settings.cityName.c_str(), sizeof(appData.city));
        appData.weatherValid = false;  // The old temperature is from the old city
        ui.requestRedraw();
        downloadNow = true;
    }
    if (portal.takeCalendarChanged()) {
        appData.calendarValid = false;  // Old events belong to the old link
        appData.eventCount = 0;
        ui.requestRedraw();
        downloadNow = true;
    }
    ClaudeUsage usage;
    if (portal.takeClaudeUsage(usage)) {
        applyClaudeUsage(usage);
    }
    if (portal.takeWifiChanged()) {
        strlcpy(appData.wifiName, settings.wifiSsid.c_str(), sizeof(appData.wifiName));
        downloadNow = true;
    }
}

void updateSetup() {
    if (ui.takeSettingsOpened()) {
        fillSettingsPage();
    }

    if (!portal.active()) {
        if (ui.takeSetupRequest()) {
            startSetup();  // Press on the Settings screen
            return;
        }
        // Settings pages on the home Wi-Fi (http://desk.local)
        if (WiFi.status() == WL_CONNECTED && !portal.lanRunning()) {
            portal.startLan(settings);
        }
        portal.loop();
        applySettingsChanges();
        return;
    }

    portal.loop();
    if (portal.takeChanged()) {
        strlcpy(appData.setupStatus, portal.statusText().c_str(), sizeof(appData.setupStatus));
        ui.requestRedraw();
    }

    if (ui.takeSetupRestart()) {
        portal.end();
        startSetup();
        return;
    }

    if (ui.takeSetupCancel()) {
        portal.end();
        appData.setupActive = false;
        ui.closeSetup();
        connectWiFi(false);  // Back to the saved Wi-Fi
        return;
    }

    if (portal.finished()) {
        portal.end();
        appData.setupActive = false;
        appData.wifiConnected = WiFi.status() == WL_CONNECTED;
        ui.closeSetup();
        downloadNow = true;
    }

}

void initTime() {
    // ESP32 keeps syncing with the NTP server in the background
    configTzTime(TIME_ZONE, NTP_SERVER);
}

// ---------- Feature functions ----------

void updateTime() {
    struct tm now;
    if (!getLocalTime(&now, 0)) {
        return;  // Not synced with NTP yet
    }

    bool minuteChanged = !appData.timeValid || now.tm_min != appData.now.tm_min;
    appData.now = now;
    appData.epoch = time(nullptr);
    appData.timeValid = true;
    appData.wifiConnected = (WiFi.status() == WL_CONNECTED);

    if (minuteChanged) {
        Serial.println(&now, "Time: %H:%M  %a %d %b %Y");
        ui.requestRedraw();
    }
}

void readSensor() {
    static unsigned long lastRead = 0;
    if (!sensorFound) {
        return;
    }
    if (appData.sensorValid && millis() - lastRead < SENSOR_INTERVAL_MS) {
        return;
    }

    bool dataReady = false;
    if (scd4x.getDataReadyStatus(dataReady) != 0 || !dataReady) {
        return;  // New value every 5 s, try again on the next loop
    }
    lastRead = millis();

    uint16_t co2;
    float temperature, humidity;
    if (scd4x.readMeasurement(co2, temperature, humidity) != 0) {
        Serial.println("SCD-41 read failed");
        return;
    }

    Serial.printf("Indoor: %.1f C  %.1f %%RH  %u ppm\n", temperature, humidity, co2);

    // Only redraw when a value shown on screen changes (whole numbers)
    bool changed = !appData.sensorValid ||
                   lroundf(temperature) != lroundf(appData.indoorTemp) ||
                   lroundf(humidity) != lroundf(appData.humidity);

    appData.indoorTemp = temperature;
    appData.humidity = humidity;
    appData.co2 = co2;
    appData.sensorValid = true;

    if (changed) {
        ui.requestRedraw();
    }
}

void updateDisplay() {
    if (millis() < FIRST_DRAW_DELAY_MS) {
        return;
    }
    ui.update();
    display.tick();  // Powers the panel down after EPD_KEEP_POWER_MS
}

void handleInput() {
    static unsigned long lastInput = 0;
    static bool touchSeen = false;  // The touch module can't be detected, only used

    if (touchPending) {
        touchPending = false;
        touchSeen = true;
        lastInput = millis();
        ui.onTouch();
    }

    // Sleep only if something can wake the screen up again
    bool canWake = encoderFound || touchSeen;
    if (canWake && millis() - lastInput >= SLEEP_AFTER_MS) {
        ui.enterSleep();
    }

    if (!encoderFound) {
        return;
    }

    int32_t steps = rotary.getEncoderDelta();
    if (steps != 0) {
        lastInput = millis();
        ui.onRotate(steps);
    }

    // Short press fires on release, long press fires once while still held
    static bool wasPressed = false;
    static bool longPressSent = false;
    static unsigned long pressStart = 0;

    bool pressed = !rotary.digitalRead(ENCODER_BUTTON_PIN);  // Active low

    if (pressed) {
        lastInput = millis();
    }

    if (pressed && !wasPressed) {
        pressStart = millis();
        longPressSent = false;
    } else if (pressed && !longPressSent && millis() - pressStart >= LONG_PRESS_MS) {
        longPressSent = true;
        ui.onLongPress();
    } else if (!pressed && wasPressed && !longPressSent) {
        ui.onPress();
    }

    wasPressed = pressed;
}

// Test mode: switch to the next page every DEMO_PAGE_MS, forever.
// Called right after updateDisplay(), so the timer starts once the
// slow E-Ink refresh has finished and every page is visible for the full time.
void showPagesInTurn() {
    static bool pageJustChanged = true;  // Home is the first page drawn
    static unsigned long pageShownAt = 0;

    if (millis() < FIRST_DRAW_DELAY_MS) {
        return;
    }

    if (pageJustChanged) {
        pageShownAt = millis();
        pageJustChanged = false;
        return;
    }

    if (millis() - pageShownAt >= DEMO_PAGE_MS) {
        Serial.println("Demo: next page");
        ui.showNextPage();
        pageJustChanged = true;
    }
}

void updateWeather(bool force) {
    static unsigned long lastAttempt = 0;
    static bool everAttempted = false;

    if (WiFi.status() != WL_CONNECTED || portal.active()) {
        return;
    }

    // Downloads block for a moment: wait while the controls are in use
    if (!force && ui.inUse()) {
        return;
    }

    unsigned long interval = appData.weatherValid ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS;
    if (!force && everAttempted && millis() - lastAttempt < interval) {
        return;
    }
    lastAttempt = millis();
    everAttempted = true;

    String response;
    String requestError;
    if (!fetchWeather(settings.cityLat, settings.cityLon, WEATHER_API_KEY, response, requestError)) {
        Serial.printf("Weather request failed: %s\n", requestError.c_str());
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
        Serial.printf("Weather JSON error: %s\n", error.c_str());
        return;
    }

    float temperature = doc["main"]["temp"] | NAN;
    int code = doc["weather"][0]["id"] | 0;
    const char* city = doc["name"] | "unknown";
    if (!isfinite(temperature) || doc["weather"][0]["id"].isNull()) {
        Serial.println("Weather JSON error: response is missing temperature or condition code");
        return;
    }
    Serial.printf("Weather: %s, %.1f C, code %d\n", city, temperature, code);

    bool changed = !appData.weatherValid ||
                   lroundf(temperature) != lroundf(appData.outdoorTemp) ||
                   code != appData.weatherCode;

    appData.outdoorTemp = temperature;
    appData.weatherCode = code;
    appData.weatherValid = true;

    if (changed || force) {
        ui.requestRedraw();
    }
}

static bool sameEvents(const CalendarEvent* a, const CalendarEvent* b, int count) {
    for (int i = 0; i < count; i++) {
        if (a[i].day != b[i].day || a[i].startMin != b[i].startMin || a[i].endMin != b[i].endMin ||
            a[i].allDay != b[i].allDay || strcmp(a[i].title, b[i].title) != 0 ||
            strcmp(a[i].location, b[i].location) != 0) {
            return false;
        }
    }
    return true;
}

void updateCalendar(bool force) {
    static unsigned long lastAttempt = 0;
    static bool everAttempted = false;
    static bool warnedNoUrl = false;

    // Needs the date to know which days to load
    if (WiFi.status() != WL_CONNECTED || !appData.timeValid || portal.active()) {
        return;
    }

    // Downloads block for a moment: wait while the controls are in use
    if (!force && ui.inUse()) {
        return;
    }
    if (settings.calendarUrl.length() == 0) {
        if (!warnedNoUrl) {
            Serial.println("Calendar skipped: add the calendar link on the setup page (menu > Settings)");
            warnedNoUrl = true;
        }
        return;
    }
    warnedNoUrl = false;

    unsigned long interval = appData.calendarValid ? CALENDAR_INTERVAL_MS : CALENDAR_RETRY_MS;
    if (!force && everAttempted && millis() - lastAttempt < interval) {
        return;
    }
    lastAttempt = millis();
    everAttempted = true;

    // From this week's Monday (the calendar page shows this week) to
    // 8 weeks ahead (for the next event on Home and in the menu)
    int32_t today = DateUtil::dayOf(appData.now);
    int32_t firstDay = today - DateUtil::weekday(today);
    int32_t lastDay = today + CALENDAR_DAYS_AHEAD;

    // Download into a temporary list, so a failed update keeps the old events
    std::unique_ptr<CalendarEvent[]> events(new (std::nothrow) CalendarEvent[MAX_CALENDAR_EVENTS]);
    if (!events) {
        Serial.println("Calendar: not enough RAM");
        return;
    }

    String error;
    int32_t completeUntil = lastDay;
    int count = fetchCalendar(settings.calendarUrl.c_str(), firstDay, lastDay, events.get(), MAX_CALENDAR_EVENTS,
                              completeUntil, error);
    if (count < 0) {
        Serial.printf("Calendar request failed: %s\n", error.c_str());
        return;
    }

    // Days after completeUntil may be missing events: treat them as unknown
    lastDay = completeUntil;
    bool changed = !appData.calendarValid || count != appData.eventCount ||
                   firstDay != appData.calendarFirstDay || lastDay != appData.calendarLastDay ||
                   !sameEvents(events.get(), appData.events, count);

    for (int i = 0; i < count; i++) {
        appData.events[i] = events[i];
    }
    appData.eventCount = count;
    appData.calendarFirstDay = firstDay;
    appData.calendarLastDay = lastDay;
    appData.calendarValid = true;

    if (changed || force) {
        ui.requestRedraw();
    }
}
