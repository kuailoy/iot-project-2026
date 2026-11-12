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

// Electricity spot price for 15 minutes from `start` (spot-hinta.fi)
struct PricePoint {
    time_t start = 0;      // UTC
    float cents = 0;       // c/kWh including VAT (25.5 %), can be negative
};

// Today and tomorrow: 96 quarter-hours each, 92 or 100 on a summer-time change day
constexpr int MAX_PRICES = 200;

// About 15 KB of RAM. Enough for 8 weeks of a full student timetable.
constexpr int MAX_CALENDAR_EVENTS = 200;

// Events are loaded from this week's Monday to this many days ahead
constexpr int CALENDAR_DAYS_AHEAD = 7 * 8;

// GitHub's contribution calendar covers the last year, in at most 53 weeks
constexpr int GITHUB_MAX_DAYS = 53 * 7;

// Contribution graph of the GitHub user whose token is saved (GitHubService.h).
// Helpers to read it: GitHubData.h
struct GitHubContributions {
    char login[40] = "";
    uint32_t total = 0;         // Contributions in these days ("in the last year")
    int32_t lastDay = 0;        // Newest day as a day number (DateUtil.h), usually today
    uint16_t lastDayCount = 0;  // Contributions on that day
    uint16_t dayCount = 0;
    uint8_t levels[GITHUB_MAX_DAYS] = {};  // 0 = none ... 4 = most, like GitHub's greens; oldest first
};

// Shared data between modules and the UI.
// Modules (time, weather, sensor, calendar) write here,
// UI pages only read. Each group has a "valid" flag so the
// UI can show "--" until real data has arrived.
struct AppData {
    // ---------- Time (NTP) ----------
    bool timeValid = false;
    struct tm now = {};
    time_t epoch = 0;              // Same moment, seconds since 1970 (UTC)

    // ---------- Outdoor weather (OpenWeatherMap) ----------
    bool weatherValid = false;
    float outdoorTemp = 0;
    int weatherCode = 0;       // OpenWeatherMap condition code
    float feelsLike = 0;       // C
    float windSpeed = 0;       // m/s
    int windDeg = -1;          // Where the wind comes from, 0 = north, 90 = east; -1 = unknown
    time_t sunrise = 0;        // Today, UTC; 0 = none (polar day or night) or unknown
    time_t sunset = 0;
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

    // ---------- Electricity spot price (spot-hinta.fi, see ElectricityData.h) ----------
    bool pricesValid = false;
    int priceCount = 0;
    PricePoint prices[MAX_PRICES];  // Sorted by time

    // ---------- GitHub contributions (GitHub widget) ----------
    bool githubTokenSet = false;   // A token is saved; without one there is nothing to load
    bool githubValid = false;      // Downloaded at least once with this token
    bool githubTokenRefused = false;  // GitHub didn't accept the token at the last try
    GitHubContributions github;

    // ---------- Now playing (YouTube Music, browser extension, see NowPlaying.h) ----------
    // Helpers to read this: MusicData.h
    bool musicValid = false;       // A message came at least once
    bool musicOpen = false;        // The YouTube Music tab is open (false: closed, last song kept)
    bool musicPlaying = false;     // false = paused
    char musicTitle[64] = "";
    char musicArtist[48] = "";
    char musicAlbum[48] = "";
    uint16_t musicPosition = 0;    // Seconds into the track when the message came
    uint16_t musicDuration = 0;    // Seconds, 0 = unknown
    time_t musicUpdated = 0;       // When the last message came (UTC)

    // ---------- Claude plan usage (browser extension, see ClaudeUsage.h) ----------
    bool claudeValid = false;      // Numbers received at least once
    uint8_t claudeSession = 0;     // % of the 5-hour limit used
    uint8_t claudeWeekly = 0;      // % of the weekly limit used
    time_t claudeSessionReset = 0; // UTC, 0 = unknown
    time_t claudeWeeklyReset = 0;
    time_t claudeUpdated = 0;      // When the numbers arrived (UTC)

    // ---------- Status ----------
    bool wifiConnected = false;
    int batteryPercent = 100;
};
