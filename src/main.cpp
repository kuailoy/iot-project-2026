
#include <Arduino.h>

// Core: communication and networking
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <memory>
#include <new>

// Power: deep sleep while "turned off", woken by the touch button
#include <driver/rtc_io.h>
#include <esp_sleep.h>

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
#include "services/Downloader.h"
#include "app/DateUtil.h"
#include "app/FetchSchedule.h"
#include "app/MusicData.h"
#include "app/PressGesture.h"
#include "app/HomePages.h"
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

// The weather city is chosen on the settings page (desk.local).
// How often weather, calendar, prices and GitHub are downloaded: app/FetchSchedule.h

const unsigned long SENSOR_INTERVAL_MS = 30 * 1000UL;
const unsigned long LONG_PRESS_MS = 800;
const unsigned long POWER_HOLD_MS = 3000;  // Touch button held this long on a home page: turn off
const unsigned long SLEEP_AFTER_MS = 5 * 60 * 1000UL;  // No input for 5 min

// The first (slow) E-Ink refresh waits for the clock, at most this long
// (without Wi-Fi after a power loss the time is unknown until NTP)
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
Downloader downloader;  // Weather, calendar, prices and GitHub, in a task of their own
bool downloadNow = false;  // Fetch weather and calendar on the next loop

// Settings version per download kind, raised when the city, calendar link or
// GitHub token changes: a result asked for with the old one is dropped
uint32_t downloadGeneration[DownloadJob::KIND_COUNT] = {};

// Set by the touch interrupt, so a tap during a slow E-Ink refresh is not lost.
// It also measures how long the pin was HIGH: a finger keeps it HIGH for
// 60 ms or more, a spike (E-Ink or Wi-Fi power) only a few ms. Spikes taken
// for taps would skip pages.
volatile bool touchPending = false;
volatile uint32_t touchRiseMs = 0;
volatile uint32_t touchPulseMs = 0;  // Length of the last finished touch, 0 while touching
const uint32_t MIN_TOUCH_MS = 30;    // Shorter: not a finger

// Where the home Wi-Fi was found last time. Kept in RTC memory through the
// deep sleep of "turned off" (not after a power loss): connecting straight to
// that router on that channel skips the scan of every channel.
struct WifiShortcut {
    bool valid;
    char ssid[33];
    uint8_t bssid[6];
    int32_t channel;
};
RTC_DATA_ATTR WifiShortcut wifiShortcut = {};
volatile bool wifiShortcutTrying = false;  // Connecting with it now
volatile bool wifiShortcutFailed = false;  // Not found there: scan (set in the Wi-Fi task)

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
void updatePrices(bool force);
void updateGitHub(bool force);
void updateCitySearch();
void applyDownloads();
void powerOff();
void showPagesInTurn();
void connectWiFi();
void keepWiFi();
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void startSetup();
void updateSetup();
void applyWidgets();
void applySettingsChanges();
void applyClaudeUsage(const ClaudeUsage& usage);
void applyNowPlaying(const NowPlaying& song);
void fillSettingsPage();

// ---------- Setup ----------

void setup() {
    initSerial();
    initI2C();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Turned on with the touch button");
    }
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
    downloader.begin();
}

// ---------- Main loop ----------

void loop() {
#if UI_DEMO_MODE
    showPagesInTurn();
#else
    handleInput();
#endif
    updateSetup();
    keepWiFi();
    updateTime();
    readSensor();
    applyDownloads();
    bool refresh = ui.takeRefreshRequest() || downloadNow;
    downloadNow = false;
    updateWeather(refresh);
    updateCalendar(refresh);
    updatePrices(refresh);
    updateGitHub(refresh);
    updateCitySearch();
    updateDisplay();

    if (ui.readyToPowerOff()) {
        powerOff();  // Does not return: turning on again restarts from setup()
    }

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

    // Asleep if the device was turned off (powerOff); harmless otherwise
    scd4x.wakeUp();

    // Sensor may still be measuring after a soft reset, stop it first.
    // Not after turning on: powerOff() already stopped it.
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
        scd4x.stopPeriodicMeasurement();
        delay(500);
    }

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

// On both edges: touched (HIGH) and let go (LOW)
void IRAM_ATTR onTouchInterrupt() {
    uint32_t now = millis();
    if (digitalRead(TOUCH_PIN) == HIGH) {
        touchRiseMs = now;
        touchPulseMs = 0;
        touchPending = true;
    } else {
        uint32_t length = now - touchRiseMs;
        touchPulseMs = length > 0 ? length : 1;
    }
}

void initTouch() {
    // After being turned off the pin is still set up for waking (powerOff)
    rtc_gpio_deinit((gpio_num_t)TOUCH_PIN);

    // Pull-down keeps the pin LOW if the module is not connected
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), onTouchInterrupt, CHANGE);
    Serial.println("Touch button ready");
}

void initWiFi() {
    SettingsStore::load(settings);
    applyWidgets();
    strlcpy(appData.city, settings.cityName.c_str(), sizeof(appData.city));
    appData.githubTokenSet = settings.githubToken.length() > 0;

    // First start (or after a reset): no home Wi-Fi yet, show the setup page
    if (!settings.hasWifi()) {
        Serial.println("Wi-Fi: not set up yet");
        startSetup();
        return;
    }
    WiFi.onEvent(onWiFiEvent);
    connectWiFi();
}

// Connect to the saved home Wi-Fi in the background (the loop and the screen
// go on); updateTime() notices when it is connected
void connectWiFi() {
    const char* ssid = settings.wifiSsid.c_str();
    WiFi.mode(WIFI_STA);
    if (wifiShortcut.valid && strcmp(wifiShortcut.ssid, ssid) == 0) {
        Serial.printf("Connecting to %s (channel %d, like last time)\n", ssid, wifiShortcut.channel);
        wifiShortcutTrying = true;
        WiFi.begin(ssid, settings.wifiPassword.c_str(), wifiShortcut.channel, wifiShortcut.bssid);
    } else {
        Serial.printf("Connecting to %s\n", ssid);
        wifiShortcutTrying = false;
        WiFi.begin(ssid, settings.wifiPassword.c_str());
    }
}

// Remember the router and channel for the next start (see WifiShortcut)
void saveWifiShortcut() {
    const uint8_t* bssid = WiFi.BSSID();
    if (bssid == nullptr) {
        return;
    }
    memcpy(wifiShortcut.bssid, bssid, sizeof(wifiShortcut.bssid));
    wifiShortcut.channel = WiFi.channel();
    strlcpy(wifiShortcut.ssid, settings.wifiSsid.c_str(), sizeof(wifiShortcut.ssid));
    wifiShortcut.valid = true;
}

// Log why the Wi-Fi was lost or could not connect (once per new reason).
// Common reasons: 201 network not found, 15 / 204 wrong password,
// 200 / 8 the network went away. Runs in the Wi-Fi task.
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    static int lastReason = -1;
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        if (wifiShortcutTrying) {
            wifiShortcutTrying = false;
            wifiShortcutFailed = true;  // keepWiFi() searches every channel now
        }
        int reason = info.wifi_sta_disconnected.reason;
        if (reason != lastReason) {
            Serial.printf("Wi-Fi: not connected, reason %d\n", reason);
            lastReason = reason;
        }
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        wifiShortcutTrying = false;
        Serial.printf("Wi-Fi: connected, IP %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
        lastReason = -1;
    }
}

// The ESP32 gives up after some errors (e.g. the network was not found),
// so if the home Wi-Fi is lost, try again every 30 s. Not during setup or
// while the settings page moves the device to another network.
void keepWiFi() {
    static unsigned long lastTry = 0;
    static bool shortcutSaved = false;
    const unsigned long RETRY_MS = 30000;
    if (!settings.hasWifi() || portal.active() || portal.switchingWifi()) {
        lastTry = millis();
        shortcutSaved = false;
        wifiShortcutFailed = false;
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (!shortcutSaved) {
            saveWifiShortcut();  // Once per connection
            shortcutSaved = true;
        }
        lastTry = millis();  // The first try comes 30 s after the Wi-Fi is lost
        return;
    }
    shortcutSaved = false;

    // The router was not where it was last time: search every channel right away
    if (wifiShortcutFailed) {
        wifiShortcutFailed = false;
        wifiShortcut.valid = false;
        lastTry = millis();
        Serial.println("Wi-Fi: not found like last time, searching every channel");
        WiFi.disconnect();
        connectWiFi();
        return;
    }
    if (millis() - lastTry < RETRY_MS) {
        return;
    }
    lastTry = millis();
    Serial.printf("Wi-Fi: trying %s again\n", settings.wifiSsid.c_str());
    WiFi.disconnect();
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
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

// The saved page list, with every widget page placed
void applyWidgets() {
    HomePages::place(appData, settings.widgets.c_str());
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

// What YouTube Music plays, from the browser extension. It sends every 15 s,
// but the screen is only redrawn when something it shows changes: the song,
// play/pause, or the tab opening or closing. The time into the song moves
// on by itself (MusicData.h) and shows at the next minute's redraw.
void applyNowPlaying(const NowPlaying& song) {
    bool open = song.title[0] != '\0';
    bool wasActive = MusicData::active(appData);
    bool changed = !appData.musicValid || open != appData.musicOpen || song.playing != appData.musicPlaying ||
                   (open && (strcmp(song.title, appData.musicTitle) != 0 ||
                             strcmp(song.artist, appData.musicArtist) != 0 ||
                             strcmp(song.album, appData.musicAlbum) != 0));

    appData.musicValid = true;
    appData.musicOpen = open;
    appData.musicPlaying = open && song.playing;
    if (open) {  // A closed tab keeps the last song, for "last played"
        strlcpy(appData.musicTitle, song.title, sizeof(appData.musicTitle));
        strlcpy(appData.musicArtist, song.artist, sizeof(appData.musicArtist));
        strlcpy(appData.musicAlbum, song.album, sizeof(appData.musicAlbum));
        appData.musicPosition = song.position;
        appData.musicDuration = song.duration;
    }
    appData.musicUpdated = time(nullptr);

    if (changed) {
        Serial.printf("Music: %s%s - %s\n", open ? (song.playing ? "" : "(paused) ") : "(closed) ",
                      appData.musicArtist, appData.musicTitle);
    }
    if (changed || wasActive != MusicData::active(appData)) {
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
        downloadGeneration[DownloadJob::WEATHER]++;
        ui.requestRedraw();
        downloadNow = true;
    }
    if (portal.takeCalendarChanged()) {
        appData.calendarValid = false;  // Old events belong to the old link
        appData.eventCount = 0;
        downloadGeneration[DownloadJob::CALENDAR]++;
        ui.requestRedraw();
        downloadNow = true;
    }
    if (portal.takeGithubChanged()) {
        appData.githubTokenSet = settings.githubToken.length() > 0;
        appData.githubValid = false;  // The graph may be another user's
        appData.githubTokenRefused = false;
        downloadGeneration[DownloadJob::GITHUB]++;
        ui.requestRedraw();
        downloadNow = true;
    }
    ClaudeUsage usage;
    if (portal.takeClaudeUsage(usage)) {
        applyClaudeUsage(usage);
    }
    NowPlaying song;
    if (portal.takeNowPlaying(song)) {
        applyNowPlaying(song);
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
        connectWiFi();  // Back to the saved Wi-Fi
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

// The first draw waits only for the clock: right away after turning on (the
// ESP32 keeps the time through deep sleep), else once NTP has answered, at
// most FIRST_DRAW_DELAY_MS. Weather, sensor and calendar are drawn as they
// come; values that arrive during a refresh go into the next one together.
void updateDisplay() {
    static bool started = false;
    if (!started) {
        if (!appData.timeValid && !appData.setupActive && millis() < FIRST_DRAW_DELAY_MS) {
            return;
        }
        started = true;
    }
    ui.update();
    display.tick();  // Powers the panel down after EPD_KEEP_POWER_MS
}

void handleInput() {
    static unsigned long lastInput = 0;
    static bool touchSeen = false;  // The touch module can't be detected, only used

    // Touch button: a tap fires on release, a hold of POWER_HOLD_MS fires once
    // while still held. The interrupt catches taps that start and end while
    // the loop is busy: the pin is LOW again by now, so they count as taps.
    static PressGesture touch(POWER_HOLD_MS);

    if (touchPending) {
        touchPending = false;
        touchSeen = true;
        if (!touch.isDown()) {
            touch.update(true, millis());
            ui.onTouchDown();
        }
    }
    if (touch.isDown()) {
        lastInput = millis();
        PressGesture::Event event = touch.update(digitalRead(TOUCH_PIN) == HIGH, millis());
        uint32_t pulse = touchPulseMs;  // 0: the interrupt hasn't seen the release yet
        if (event == PressGesture::TAP && pulse != 0 && pulse < MIN_TOUCH_MS) {
            Serial.printf("Touch: ignored a %lu ms spike\n", (unsigned long)pulse);
        } else if (event == PressGesture::TAP) {
            Serial.printf("Touch: tap (%lu ms)\n", (unsigned long)pulse);
            ui.onTouch();
        } else if (event == PressGesture::HOLD) {
            ui.onTouchHold();
        }
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
    static PressGesture knob(LONG_PRESS_MS);

    bool pressed = !rotary.digitalRead(ENCODER_BUTTON_PIN);  // Active low
    if (pressed) {
        lastInput = millis();
    }
    PressGesture::Event event = knob.update(pressed, millis());
    if (event == PressGesture::TAP) {
        ui.onPress();
    } else if (event == PressGesture::HOLD) {
        ui.onLongPress();
    }
}

// ---------- Downloads ----------
// The update*() functions only decide when to download and hand a job to the
// download task (Downloader.h); apply*() put the finished results into
// AppData. So the loop, the controls and the screen never wait for the network.

// A new job of this kind, asked for with the current settings
std::unique_ptr<DownloadJob> newJob(DownloadJob::Kind kind, bool force) {
    std::unique_ptr<DownloadJob> job(new DownloadJob(kind));
    job->force = force;
    job->generation = downloadGeneration[kind];
    return job;
}

// GitHub contribution graph for the GitHub widget
void updateGitHub(bool force) {
    static FetchSchedule::Timer timer;

    if (WiFi.status() != WL_CONNECTED || portal.active() || settings.githubToken.length() == 0 ||
        downloader.busy(DownloadJob::GITHUB)) {
        return;
    }
    if (!timer.due(millis(), FetchSchedule::githubInterval(appData), force)) {
        return;
    }
    std::unique_ptr<DownloadJob> job = newJob(DownloadJob::GITHUB, force);
    job->key = settings.githubToken;
    downloader.start(std::move(job));
}

// A failed download keeps the old graph
void applyGitHub(const DownloadJob& job) {
    if (job.tokenRefused != appData.githubTokenRefused) {
        appData.githubTokenRefused = job.tokenRefused;  // The widget says so instead of "Loading..."
        ui.requestRedraw();
    }
    if (!job.ok) {
        Serial.printf("GitHub request failed: %s\n", job.error.c_str());
        return;
    }

    const GitHubContributions& fresh = *job.github;
    const GitHubContributions& old = appData.github;
    bool changed = !appData.githubValid || fresh.total != old.total || fresh.lastDay != old.lastDay ||
                   fresh.lastDayCount != old.lastDayCount || fresh.dayCount != old.dayCount ||
                   strcmp(fresh.login, old.login) != 0 ||
                   memcmp(fresh.levels, old.levels, sizeof(old.levels)) != 0;
    appData.github = fresh;
    appData.githubValid = true;
    Serial.printf("GitHub: %s, %lu contributions in the last year, %u today\n", fresh.login,
                  (unsigned long)fresh.total, fresh.lastDayCount);

    if (changed || job.force) {
        ui.requestRedraw();
    }
}

// Turn off: the "turned off" page is on the screen (E-Ink keeps it without
// power). Stop the sensor and the panel, then deep sleep until the touch
// button is touched again. Waking up restarts the device from setup().
void powerOff() {
    Serial.println("Power: turning off, touch the button to turn on");

    // A finger still on the button would wake the device right away
    while (digitalRead(TOUCH_PIN) == HIGH) {
        delay(20);
    }
    detachInterrupt(digitalPinToInterrupt(TOUCH_PIN));

    if (sensorFound) {
        scd4x.stopPeriodicMeasurement();
        delay(500);          // The sensor needs this before the next command
        scd4x.powerDown();   // initSensor() wakes it up again
    }
    display.sleep();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Wake when the pin goes HIGH (touched). The pull-down keeps it LOW if
    // the module is unplugged, so a loose pin can't turn the device on.
    rtc_gpio_pullup_dis((gpio_num_t)TOUCH_PIN);
    rtc_gpio_pulldown_en((gpio_num_t)TOUCH_PIN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1);

    Serial.flush();
    esp_deep_sleep_start();
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
    static FetchSchedule::Timer timer;

    if (WiFi.status() != WL_CONNECTED || portal.active() || downloader.busy(DownloadJob::WEATHER)) {
        return;
    }
    if (!timer.due(millis(), FetchSchedule::weatherInterval(appData), force)) {
        return;
    }
    std::unique_ptr<DownloadJob> job = newJob(DownloadJob::WEATHER, force);
    job->key = WEATHER_API_KEY;
    job->latitude = settings.cityLat;
    job->longitude = settings.cityLon;
    downloader.start(std::move(job));
}

void applyWeather(const DownloadJob& job) {
    if (!job.ok) {
        Serial.printf("Weather request failed: %s\n", job.error.c_str());
        return;
    }
    const Weather& weather = job.weather;
    Serial.printf("Weather: %s, %.1f C (feels %.1f), wind %.1f m/s from %d, code %d\n", appData.city,
                  weather.temperature, weather.feelsLike, weather.windSpeed, weather.windDeg, weather.code);

    bool changed = storeWeather(weather, appData);
    if (changed || job.force) {
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
    static FetchSchedule::Timer timer;
    static bool warnedNoUrl = false;

    // Needs the date to know which days to load
    if (WiFi.status() != WL_CONNECTED || !appData.timeValid || portal.active() ||
        downloader.busy(DownloadJob::CALENDAR)) {
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

    if (!timer.due(millis(), FetchSchedule::calendarInterval(appData), force)) {
        return;
    }

    // From this week's Monday (the calendar page shows this week) to
    // 8 weeks ahead (for the next event on Home and in the menu)
    std::unique_ptr<DownloadJob> job = newJob(DownloadJob::CALENDAR, force);
    int32_t today = DateUtil::dayOf(appData.now);
    job->firstDay = today - DateUtil::weekday(today);
    job->lastDay = today + CALENDAR_DAYS_AHEAD;
    job->url = settings.calendarUrl;
    downloader.start(std::move(job));
}

// Downloaded into a list of its own, so a failed update keeps the old events
void applyCalendar(const DownloadJob& job) {
    if (!job.ok) {
        Serial.printf("Calendar request failed: %s\n", job.error.c_str());
        return;
    }

    // Days after completeUntil may be missing events: treat them as unknown
    int count = job.eventCount;
    bool changed = !appData.calendarValid || count != appData.eventCount ||
                   job.firstDay != appData.calendarFirstDay || job.completeUntil != appData.calendarLastDay ||
                   !sameEvents(job.events.get(), appData.events, count);

    for (int i = 0; i < count; i++) {
        appData.events[i] = job.events[i];
    }
    appData.eventCount = count;
    appData.calendarFirstDay = job.firstDay;
    appData.calendarLastDay = job.completeUntil;
    appData.calendarValid = true;

    if (changed || job.force) {
        ui.requestRedraw();
    }
}

// ---------- Electricity spot price (api.spot-hinta.fi) ----------

// When to ask (rarely, prices are fixed once published): FetchSchedule::pricesInterval
void updatePrices(bool force) {
    static FetchSchedule::Timer timer;

    if (WiFi.status() != WL_CONNECTED || !appData.timeValid || portal.active() ||
        downloader.busy(DownloadJob::PRICES)) {
        return;
    }
    if (!timer.due(millis(), FetchSchedule::pricesInterval(appData), force, FetchSchedule::PRICES_MIN_GAP_MS)) {
        return;
    }
    downloader.start(newJob(DownloadJob::PRICES, force));
}

void applyPrices(const DownloadJob& job) {
    if (!job.ok) {
        Serial.printf("Price request failed: %s\n", job.error.c_str());
        return;
    }

    int count = job.priceCount;
    const PricePoint* prices = job.prices.get();
    bool changed = !appData.pricesValid || count != appData.priceCount;
    for (int i = 0; i < count && !changed; i++) {
        changed = prices[i].start != appData.prices[i].start || prices[i].cents != appData.prices[i].cents;
    }
    for (int i = 0; i < count; i++) {
        appData.prices[i] = prices[i];
    }
    appData.priceCount = count;
    appData.pricesValid = true;
    int32_t today = DateUtil::dayOf(appData.now);
    Serial.printf("Prices: %d quarter-hours, tomorrow %s\n", count,
                  ElectricityData::hasDay(appData, today + 1) ? "yes" : "not yet");

    if (changed) {
        ui.requestRedraw();
    }
}

// City search on the settings page (desk.local): the page asks again until
// the answer is there, so the web server never waits for OpenWeatherMap
void updateCitySearch() {
    String query;
    if (downloader.busy(DownloadJob::CITY_SEARCH) || !portal.takeCitySearch(query)) {
        return;
    }
    std::unique_ptr<DownloadJob> job = newJob(DownloadJob::CITY_SEARCH, false);
    job->key = WEATHER_API_KEY;
    job->query = query;
    downloader.start(std::move(job));
}

// Finished downloads into AppData; only the loop touches AppData
void applyDownloads() {
    while (std::unique_ptr<DownloadJob> job = downloader.takeFinished()) {
        if (job->generation != downloadGeneration[job->kind]) {
            downloadNow = true;  // Asked for with the old city, link or token: ask again
            continue;
        }
        switch (job->kind) {
            case DownloadJob::WEATHER:
                applyWeather(*job);
                break;
            case DownloadJob::CALENDAR:
                applyCalendar(*job);
                break;
            case DownloadJob::PRICES:
                applyPrices(*job);
                break;
            case DownloadJob::GITHUB:
                applyGitHub(*job);
                break;
            case DownloadJob::CITY_SEARCH:
                if (!job->ok) {
                    Serial.printf("City search failed: %s\n", job->error.c_str());
                }
                portal.citySearchDone(job->query, job->ok, job->json);
                break;
            case DownloadJob::KIND_COUNT:
                break;
        }
    }
}
