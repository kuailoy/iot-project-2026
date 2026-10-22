
#include <Arduino.h>

// Core: communication and networking
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
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

// ---------- Global objects ----------

SensirionI2cScd4x scd4x;
Adafruit_seesaw rotary;

// ---------- Function declarations ----------

void initSerial();
void initI2C();
void initWiFi();
void initTime();

void readSensor();
void updateDisplay();
void handleInput();
void updateWeather();

// ---------- Setup ----------

void setup() {
    initSerial();
    initI2C();

    Serial.println("System starting...");

    // Enable these when implementing each feature:
    // initWiFi();
    // initTime();
}

// ---------- Main loop ----------

void loop() {
    handleInput();

    // readSensor();
    // updateWeather();
    // updateDisplay();

    delay(100);
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

void initWiFi() {
    // TODO: Connect to Wi-Fi
}

void initTime() {
    // TODO: Synchronize time using NTP
}

// ---------- Feature functions ----------

void readSensor() {
    // TODO: Read SCD-41 measurements
}

void updateDisplay() {
    // TODO: Render content on the E-Ink display
}

void handleInput() {
    // TODO: Read rotary encoder and touch input
}

void updateWeather() {
    // TODO: Fetch and parse Open-Meteo data
}
