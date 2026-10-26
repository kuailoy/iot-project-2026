#pragma once

#include <stdint.h>
#include <time.h>

// Shared data between modules and the UI.
// Modules (time, weather, sensor, calendar) write here,
// UI pages only read. Each group has a "valid" flag so the
// UI can show "--" until real data has arrived.
struct AppData {
    // ---------- Time (NTP) ----------
    bool timeValid = false;
    struct tm now = {};

    // ---------- Outdoor weather (Open-Meteo) ----------
    bool weatherValid = false;
    float outdoorTemp = 0;
    int weatherCode = 0;       // WMO code, 0 = clear sky
    char city[24] = "Kuopio";

    // ---------- Indoor sensor (SCD-41) ----------
    bool sensorValid = false;
    float indoorTemp = 0;
    float humidity = 0;
    uint16_t co2 = 0;          // ppm

    // ---------- Calendar ----------
    bool eventValid = false;
    char eventTime[6] = "";    // "10:30"
    char eventTitle[40] = "";

    // ---------- Status ----------
    bool wifiConnected = false;
    int batteryPercent = 100;
};
