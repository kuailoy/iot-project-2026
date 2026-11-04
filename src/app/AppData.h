#pragma once

#include <stdint.h>
#include <time.h>

#include "WidgetLayout.h"

// One occurrence of a calendar event, already in local time.
// Recurring events are expanded: every occurrence is its own entry.
struct CalendarEvent {
    int32_t day = 0;         // Local date as a day number (see DateUtil.h)
    int16_t startMin = 0;    // Minutes after midnight
    int16_t endMin = 0;      // Minutes after midnight, 1440 if it runs past midnight
    bool allDay = false;
    char title[40] = "";
    char location[24] = "";  // Room or place, "" if none
};

// About 15 KB of RAM. Enough for 8 weeks of a full student timetable.
constexpr int MAX_CALENDAR_EVENTS = 200;

// Events are loaded from this week's Monday to this many days ahead
constexpr int CALENDAR_DAYS_AHEAD = 7 * 8;

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
    int weatherCode = 0;       // OpenWeatherMap condition code
    char city[24] = "Kuopio";

    // ---------- Indoor sensor (SCD-41) ----------
    bool sensorValid = false;
    float indoorTemp = 0;
    float humidity = 0;
    uint16_t co2 = 0;          // ppm

    // ---------- Calendar (iCal feed) ----------
    // Helpers to read this: CalendarData.h
    bool calendarValid = false;      // At least one successful download
    int32_t calendarFirstDay = 0;    // Events are complete for these days (inclusive);
    int32_t calendarLastDay = 0;     // days outside are unknown, not free
    int eventCount = 0;
    CalendarEvent events[MAX_CALENDAR_EVENTS];  // Sorted by day, then start time

    // ---------- Wi-Fi setup page (SetupPortal) ----------
    bool setupActive = false;
    bool setupCanCancel = false;       // A home Wi-Fi was saved before: Press goes back
    char setupNetwork[24] = "";        // Name of the setup Wi-Fi, "Desk-Setup-4F2A"
    char setupPassword[16] = "";
    char setupStatus[64] = "";         // "Waiting for a phone to connect..."

    // ---------- Settings page and second home page ----------
    char deviceIp[16] = "";            // On the home Wi-Fi, "" when not connected
    char wifiName[33] = "";
    int widgetCount = 0;
    WidgetSlot widgets[MAX_WIDGETS];   // Placed widgets, see WidgetLayout.h

    // QR code shown on the setup or settings page
    uint8_t qrSize = 0;                       // Modules per side, 0 = no code
    uint8_t qrCode[(41 * 41 + 7) / 8] = {};   // 1 bit per module, row by row, MSB first

    // ---------- Status ----------
    bool wifiConnected = false;
    int batteryPercent = 100;
};
