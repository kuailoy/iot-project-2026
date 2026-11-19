#include "Downloader.h"

#include <ArduinoJson.h>
#include <new>

#include "CalendarService.h"
#include "ElectricityService.h"
#include "GitHubService.h"
#include "WeatherService.h"

namespace {

// TLS needs a deep stack; the loop runs on core 1, Wi-Fi on core 0
constexpr uint32_t TASK_STACK_BYTES = 16 * 1024;
constexpr UBaseType_t TASK_PRIORITY = 1;
constexpr BaseType_t TASK_CORE = 0;

const char* const KIND_NAMES[DownloadJob::KIND_COUNT] = {"weather", "calendar", "prices", "GitHub",
                                                         "city search"};

}  // namespace

void Downloader::begin() {
    // At most one job of each kind at a time, so the queues never fill up
    requests = xQueueCreate(DownloadJob::KIND_COUNT, sizeof(DownloadJob*));
    results = xQueueCreate(DownloadJob::KIND_COUNT, sizeof(DownloadJob*));
    xTaskCreatePinnedToCore(taskMain, "downloads", TASK_STACK_BYTES, this, TASK_PRIORITY, nullptr, TASK_CORE);
}

void Downloader::start(std::unique_ptr<DownloadJob> job) {
    pending[job->kind] = true;
    DownloadJob* raw = job.release();
    xQueueSend(requests, &raw, portMAX_DELAY);
}

std::unique_ptr<DownloadJob> Downloader::takeFinished() {
    DownloadJob* raw = nullptr;
    if (results == nullptr || xQueueReceive(results, &raw, 0) != pdTRUE) {
        return nullptr;
    }
    pending[raw->kind] = false;
    return std::unique_ptr<DownloadJob>(raw);
}

void Downloader::taskMain(void* self) {
    Downloader& downloader = *static_cast<Downloader*>(self);
    while (true) {
        DownloadJob* job = nullptr;
        if (xQueueReceive(downloader.requests, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        uint32_t start = millis();
        run(*job);
        job->tookMs = millis() - start;
        Serial.printf("Download: %s %s in %lu ms (free RAM %u, task stack left %u)\n", KIND_NAMES[job->kind],
                      job->ok ? "done" : "failed", (unsigned long)job->tookMs, ESP.getFreeHeap(),
                      uxTaskGetStackHighWaterMark(nullptr));
        xQueueSend(downloader.results, &job, portMAX_DELAY);
    }
}

// Runs in the download task: only the job, nothing shared
void Downloader::run(DownloadJob& job) {
    switch (job.kind) {
        case DownloadJob::WEATHER: {
            String response;
            if (!fetchWeather(job.latitude, job.longitude, job.key.c_str(), response, job.error)) {
                return;
            }
            JsonDocument doc;
            DeserializationError parseError = deserializeJson(doc, response);
            if (parseError) {
                job.error = String("JSON: ") + parseError.c_str();
            } else if (!readWeather(doc.as<JsonObjectConst>(), job.weather)) {
                job.error = "response is missing temperature or condition code";
            } else {
                job.ok = true;
            }
            return;
        }
        case DownloadJob::CALENDAR:
            job.events.reset(new (std::nothrow) CalendarEvent[MAX_CALENDAR_EVENTS]);
            if (!job.events) {
                job.error = "not enough RAM";
                return;
            }
            job.completeUntil = job.lastDay;
            job.eventCount = fetchCalendar(job.url.c_str(), job.firstDay, job.lastDay, job.events.get(),
                                           MAX_CALENDAR_EVENTS, job.completeUntil, job.error);
            job.ok = job.eventCount >= 0;
            return;
        case DownloadJob::PRICES:
            job.prices.reset(new (std::nothrow) PricePoint[MAX_PRICES]);
            if (!job.prices) {
                job.error = "not enough RAM";
                return;
            }
            job.priceCount = fetchPrices(job.prices.get(), MAX_PRICES, job.error);
            job.ok = job.priceCount >= 0;
            return;
        case DownloadJob::GITHUB:
            job.github.reset(new (std::nothrow) GitHubContributions());
            if (!job.github) {
                job.error = "not enough RAM";
                return;
            }
            job.ok = fetchContributions(job.key.c_str(), *job.github, job.error, job.tokenRefused);
            return;
        case DownloadJob::CITY_SEARCH:
            job.ok = findCities(job.query, job.key.c_str(), job.json, job.error);
            return;
        case DownloadJob::KIND_COUNT:
            return;
    }
}
