
#include <Arduino.h>

// Core: communication and networking
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

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
#include "services/CalendarService.h"
#include "services/WeatherService.h"
#include "ui/UI.h"
#include "display/EinkDisplay.h"
#include "display/EinkBenchmark.h"

// Wi-Fi credentials are injected from .env.local by the PlatformIO pre-script.
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef WEATHER_API_KEY
#define WEATHER_API_KEY ""
#endif
#ifndef CALENDAR_URL
#define CALENDAR_URL ""  // iCal feed, webcal:// or https://
#endif

// ---------- Settings ----------

const char* NTP_SERVER = "pool.ntp.org";
const char* TIME_ZONE = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // Finland, with summer time

// Location for the weather forecast (Kuopio)
const float LATITUDE = 62.89;
const float LONGITUDE = 27.68;

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

bool sensorFound = false;
bool encoderFound = false;

// ---------- Function declarations ----------

void initSerial();
void initI2C();
void initDisplay();
void initSensor();
void initEncoder();
void initWiFi();
void initTime();

void updateTime();
void readSensor();
void updateDisplay();
void handleInput();
void updateWeather(bool force);
void updateCalendar(bool force);
void updateNextEvent();
void showPagesInTurn();

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
    updateTime();
    readSensor();
    bool refresh = ui.takeRefreshRequest();
    updateWeather(refresh);
    updateCalendar(refresh);
    updateNextEvent();
    updateDisplay();

    delay(50);
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

void initWiFi() {
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("Wi-Fi skipped: WIFI_SSID is not set in .env.local");
        return;
    }

    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Give up after 15 s so the device still starts without Wi-Fi
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    appData.wifiConnected = (WiFi.status() == WL_CONNECTED);
    Serial.println(appData.wifiConnected ? " connected" : " failed");
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

    // Without the encoder there is no way to wake up, so never sleep
    if (!encoderFound) {
        return;
    }

    if (millis() - lastInput >= SLEEP_AFTER_MS) {
        ui.enterSleep();
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

    if (WiFi.status() != WL_CONNECTED) {
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
    if (!fetchWeather(LATITUDE, LONGITUDE, WEATHER_API_KEY, response, requestError)) {
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

void updateCalendar(bool force) {
    static unsigned long lastAttempt = 0;
    static bool everAttempted = false;

    // The time is needed to know which weeks to keep
    if (WiFi.status() != WL_CONNECTED || !appData.timeValid || strlen(CALENDAR_URL) == 0) {
        return;
    }

    unsigned long interval = appData.calendar.valid ? CALENDAR_INTERVAL_MS : CALENDAR_RETRY_MS;
    if (!force && everAttempted && millis() - lastAttempt < interval) {
        return;
    }
    lastAttempt = millis();
    everAttempted = true;

    // From the start of this month (or week) to as far as the Calendar page can browse
    int32_t today = CalendarDate::fromTm(appData.now);
    int32_t monthStart = today - (appData.now.tm_mday - 1);
    int32_t weekStart = today - CalendarDate::weekdayOf(today);
    int32_t fromDay = monthStart < weekStart ? monthStart : weekStart;

    static CalendarData fresh;  // About 4 KB, too big for the loop stack
    String error;
    if (!fetchCalendar(CALENDAR_URL, fromDay, today + CalendarData::DAYS_AHEAD, fresh, error)) {
        Serial.printf("Calendar request failed: %s\n", error.c_str());
        return;
    }
    Serial.printf("Calendar: %d events loaded\n", fresh.count);

    // Only redraw when an event changed
    if (!fresh.sameAs(appData.calendar)) {
        appData.calendar = fresh;
        ui.requestRedraw();
    }
}

// Home page "NEXT EVENT": the running or next event of today
void updateNextEvent() {
    if (!appData.timeValid || !appData.calendar.valid) {
        return;
    }

    int32_t today = CalendarDate::fromTm(appData.now);
    int minute = appData.now.tm_hour * 60 + appData.now.tm_min;
    const CalendarEvent* next = appData.calendar.nextAfter(today, minute);

    // The Home page redraws on the next minute anyway, so no redraw is requested here
    appData.eventValid = next != nullptr && next->day == today;
    if (!appData.eventValid) {
        return;
    }

    if (next->allDay) {
        strcpy(appData.eventTime, "Today");
    } else {
        int hour = (next->startMinute / 60) % 24;
        int min = next->startMinute % 60;
        appData.eventTime[0] = '0' + hour / 10;
        appData.eventTime[1] = '0' + hour % 10;
        appData.eventTime[2] = ':';
        appData.eventTime[3] = '0' + min / 10;
        appData.eventTime[4] = '0' + min % 10;
        appData.eventTime[5] = '\0';
    }
    snprintf(appData.eventTitle, sizeof(appData.eventTitle), "%s", next->title);
}
