
#include <Arduino.h>

// Core: communication and networking
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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
#include "ui/UI.h"

// Wi-Fi credentials: copy include/secrets.example.h to include/secrets.h
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

// ---------- Settings ----------

const char* NTP_SERVER = "pool.ntp.org";
const char* TIME_ZONE = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // Finland, with summer time

// Location for the weather forecast (Kuopio)
const float LATITUDE = 62.89;
const float LONGITUDE = 27.68;

const unsigned long WEATHER_INTERVAL_MS = 15 * 60 * 1000UL;
const unsigned long WEATHER_RETRY_MS = 60 * 1000UL;
const unsigned long SENSOR_INTERVAL_MS = 30 * 1000UL;
const unsigned long LONG_PRESS_MS = 800;
const unsigned long SLEEP_AFTER_MS = 5 * 60 * 1000UL;  // No input for 5 min

// Wait for time, weather and sensor before the first (slow) E-Ink refresh
const unsigned long FIRST_DRAW_DELAY_MS = 10 * 1000UL;

// ---------- Global objects ----------

ThinkInk_750_Tricolor_AABMFGNR display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
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

// ---------- Setup ----------

void setup() {
    initSerial();
    initI2C();

    Serial.println("System starting...");

    initDisplay();
    initSensor();
    initEncoder();
    initWiFi();
    initTime();
}

// ---------- Main loop ----------

void loop() {
    handleInput();
    updateTime();
    readSensor();
    updateWeather(ui.takeRefreshRequest());
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
    display.begin(THINKINK_TRICOLOR);
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
        Serial.println("Wi-Fi skipped: no include/secrets.h");
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

    char url[160];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f"
             "&current=temperature_2m,weather_code",
             LATITUDE, LONGITUDE);

    // TODO: Add the server's root certificate instead of skipping the check
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("Weather request failed: %d\n", status);
        http.end();
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    if (error) {
        Serial.printf("Weather JSON error: %s\n", error.c_str());
        return;
    }

    float temperature = doc["current"]["temperature_2m"];
    int code = doc["current"]["weather_code"];
    Serial.printf("Weather: %.1f C, code %d\n", temperature, code);

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
