#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <memory>

#include "../app/AppData.h"
#include "WeatherParser.h"

// One download for the download task: what to fetch, and what came back.
// Only one side uses it at a time: main.cpp fills in the request and hands
// it over, the task fills in the result and hands it back.
struct DownloadJob {
    enum Kind : uint8_t { WEATHER, CALENDAR, PRICES, GITHUB, CITY_SEARCH, KIND_COUNT };

    Kind kind;
    bool force = false;       // Asked for with "refresh": redraw even if nothing changed
    uint32_t generation = 0;  // Settings version it was asked with (main.cpp drops old ones)

    // Request
    String key;               // Weather and city search: API key; GitHub: token
    String url;               // Calendar link
    String query;             // City search: what the settings page asked for
    double latitude = 0;      // Weather city
    double longitude = 0;
    int32_t firstDay = 0;     // Calendar days to load
    int32_t lastDay = 0;

    // Result
    bool ok = false;
    String error;             // Why it failed
    uint32_t tookMs = 0;
    Weather weather;
    std::unique_ptr<CalendarEvent[]> events;  // MAX_CALENDAR_EVENTS
    int eventCount = 0;
    int32_t completeUntil = 0;
    std::unique_ptr<PricePoint[]> prices;     // MAX_PRICES
    int priceCount = 0;
    std::unique_ptr<GitHubContributions> github;
    bool tokenRefused = false;
    String json;              // City search: the places, for the page (see findCities)

    explicit DownloadJob(Kind kind) : kind(kind) {}
};

// Runs the slow downloads (weather, calendar, prices, GitHub, the settings
// page's city search) one after
// another in a task of their own on the other CPU core, so the controls and
// the screen never wait for the network. main.cpp decides what to download
// and when (FetchSchedule.h), and puts the results into AppData itself:
// the download task never touches AppData.
class Downloader {
public:
    void begin();

    // A job of this kind is waiting or running: don't ask for another one
    bool busy(DownloadJob::Kind kind) const { return pending[kind]; }

    // Hands the job to the download task
    void start(std::unique_ptr<DownloadJob> job);

    // A finished job, or nothing
    std::unique_ptr<DownloadJob> takeFinished();

private:
    static void taskMain(void* self);
    static void run(DownloadJob& job);

    QueueHandle_t requests = nullptr;
    QueueHandle_t results = nullptr;
    bool pending[DownloadJob::KIND_COUNT] = {};  // Only used by main.cpp's loop
};
