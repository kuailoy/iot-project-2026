// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#include <Adafruit_GFX.h>

#include "Components.h"
#include "../app/CalendarData.h"
#include "../app/DateUtil.h"
#include "../app/GitHubData.h"
#include "pages/CalendarPage.h"
#include "pages/HabitPage.h"
#include "../app/Habits.h"
#include "pages/ElectricityPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/PomodoroPage.h"
#include "pages/PowerOffPage.h"
#include "pages/SettingsPage.h"
#include "pages/SetupPage.h"
#include "pages/WidgetPage.h"
#include "pages/SleepPage.h"
#include "../../src/services/GitHubParser.h"
#include "../../src/services/NowPlaying.h"
#include "../app/MusicData.h"
#include "pages/MusicPage.h"
#include "../../src/services/IcsParser.h"
#include "../../src/services/WeatherParser.h"
#include "../app/FetchSchedule.h"
#include "../app/PressGesture.h"
#include "../app/ElectricityData.h"
#include "../../src/services/ClaudeUsage.h"

static bool savePbm(GFXcanvas1& canvas, const char* path) {
    FILE* file = fopen(path, "wb");
    if (file == nullptr) {
        return false;
    }

    // P4 = binary bitmap, 1 = black, rows padded to whole bytes like GFXcanvas1
    fprintf(file, "P4\n%d %d\n", canvas.width(), canvas.height());
    size_t bytes = ((canvas.width() + 7) / 8) * canvas.height();
    fwrite(canvas.getBuffer(), 1, bytes, file);
    fclose(file);
    return true;
}

// Same values as the Figma design
static AppData demoData() {
    AppData data;

    data.timeValid = true;
    data.now.tm_year = 126;  // 2026
    data.now.tm_hour = 10;
    data.now.tm_min = 22;
    data.now.tm_mday = 5;
    data.now.tm_mon = 9;   // October (0 = January)
    data.now.tm_wday = 1;  // Monday
    data.now.tm_yday = 277;

    data.weatherValid = true;
    data.outdoorTemp = 8;
    data.feelsLike = 5.4f;
    data.windSpeed = 4.2f;
    data.windDeg = 225;  // From the south-west

    data.sensorValid = true;
    data.indoorTemp = 22;
    data.humidity = 45;
    data.co2 = 640;

    // Claude plan usage like the claude.ai example: 54% session, 29% week
    // The same moment as data.now, in Finnish time (main() sets TZ)
    struct tm local = data.now;
    local.tm_isdst = -1;
    data.epoch = mktime(&local);
    data.claudeValid = true;
    data.claudeSession = 54;
    data.claudeWeekly = 29;
    data.claudeSessionReset = data.epoch + 2 * 3600 + 20 * 60;
    data.claudeWeeklyReset = data.epoch + 2 * 86400 + 6 * 3600;
    data.claudeUpdated = data.epoch - 3 * 60;

    // Electricity: hourly prices of a real day (c/kWh with VAT), each hour as
    // four quarter-hours; tomorrow a bit cheaper
    static const float HOURLY[24] = {2.27f, 1.18f, 1.21f, 1.25f, 1.12f, 1.19f, 1.73f, 1.95f,
                                     2.39f, 2.98f, 3.45f, 4.06f, 5.55f, 7.80f, 9.72f, 10.05f,
                                     27.85f, 37.16f, 41.59f, 40.54f, 35.90f, 28.47f, 25.73f, 17.20f};
    struct tm midnight = data.now;
    midnight.tm_hour = midnight.tm_min = midnight.tm_sec = 0;
    midnight.tm_isdst = -1;
    time_t dayStart = mktime(&midnight);
    for (int day = 0; day < 2; day++) {
        for (int q = 0; q < 96; q++) {
            PricePoint& p = data.prices[data.priceCount++];
            p.start = dayStart + day * 86400 + q * 900;
            float wobble = (q % 4 == 1 ? 0.4f : q % 4 == 3 ? -0.3f : 0.0f);
            p.cents = HOURLY[q / 4] * (day == 0 ? 1.0f : 0.7f) + wobble;
        }
    }
    data.pricesValid = true;

    // Kuopio on 5 Oct: sunrise 07:35, sunset 18:41
    data.sunrise = dayStart + 7 * 3600 + 35 * 60;
    data.sunset = dayStart + 18 * 3600 + 41 * 60;

    // Calendar events from the Figma designs
    struct Sample {
        int day, hour, min, length;
        const char* title;
        const char* location;
    };
    static const Sample samples[] = {
        {5, 9, 0, 90, "Finnish class", "Mic-J430"},
        {5, 10, 30, 60, "Project meeting", ""},
        {5, 12, 15, 45, "Lunch with team", "Microkatu 1"},
        {5, 14, 0, 120, "Communication Skills for Engineering Ex", "Mic-E4004"},
        {5, 16, 30, 60, "Gym", ""},
        {7, 9, 0, 90, "Finnish class", "Mic-J430"},
        {7, 13, 0, 60, "Sync", ""},
        {9, 9, 0, 90, "Finnish class", "Mic-J430"},
        {9, 15, 0, 60, "Report", ""},
        {10, 11, 0, 120, "Market", "Kauppatori"},
        {14, 9, 0, 90, "Finnish class", "Mic-J430"},
        {19, 10, 0, 60, "Dentist", ""},
        {21, 9, 0, 90, "Finnish class", "Mic-J430"},
        {27, 18, 0, 60, "Concert", ""},
        {28, 9, 0, 90, "Finnish class", "Mic-J430"},
        {30, 0, 0, 0, "Trip to Aanekoski", ""},
    };
    for (const Sample& e : samples) {
        CalendarEvent& ev = data.events[data.eventCount++];
        ev.day = DateUtil::daysFromCivil(2026, 10, e.day);
        ev.allDay = e.length == 0;
        ev.startMin = e.hour * 60 + e.min;
        ev.endMin = ev.allDay ? 1440 : ev.startMin + e.length;
        strcpy(ev.title, e.title);
        strcpy(ev.location, e.location);
    }
    data.calendarValid = true;
    data.calendarFirstDay = DateUtil::daysFromCivil(2026, 9, 28);
    data.calendarLastDay = DateUtil::daysFromCivil(2026, 11, 30);

    data.wifiConnected = true;
    return data;
}

// ---------- Pomodoro ----------

static const uint32_t MINUTE_MS = 60000UL;
static const int32_t START_CLOCK = 10 * 3600 + 23 * 60;  // First session starts 10:23

static int checksFailed = 0;  // All checks, main() fails if any did

static void check(bool ok, const char* what) {
    if (!ok) {
        printf("Check FAILED: %s\n", what);
        checksFailed++;
    }
}

static int32_t clockAt(uint32_t ms) {
    return START_CLOCK + (int32_t)(ms / 1000);
}

// Runs whole focus sessions with their breaks, returns the time after them
static uint32_t runSessions(Pomodoro& timer, int count, uint32_t t) {
    for (int i = 0; i < count; i++) {
        timer.start(t, clockAt(t));
        t += timer.focusMinutes() * MINUTE_MS;
        timer.tick(t);
        if (timer.state() == Pomodoro::FOCUS_DONE) {
            timer.start(t, clockAt(t));  // Break
            t += Pomodoro::BREAK_MINUTES * MINUTE_MS;
            timer.tick(t);
        }
    }
    return t;
}

static void drawPomodoro(GFXcanvas1& canvas, const AppData& data, const Pomodoro& timer,
                         uint32_t nowMs, const char* path) {
    PomodoroPage page;
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data, timer, nowMs);
    savePbm(canvas, path);
}

// Every timer state, also checks the timer logic on the way
static void savePomodoroPages(GFXcanvas1& canvas, const AppData& data) {
    int failedBefore = checksFailed;
    Pomodoro ready;
    drawPomodoro(canvas, data, ready, 0, "out/pomodoro-1-ready.pbm");
    ready.adjustFocus(1);
    check(ready.focusMinutes() == 30, "turning right adds 5 min");
    ready.adjustFocus(-100);
    check(ready.focusMinutes() == Pomodoro::MIN_FOCUS_MINUTES, "focus length has a minimum");

    // Session 2, 7 minutes in: 18 min left
    Pomodoro focus;
    uint32_t t = runSessions(focus, 1, 0);
    focus.start(t, clockAt(t));
    t += 7 * MINUTE_MS;
    focus.tick(t);
    check(focus.minutesLeft() == 18, "18 min left after 7 min");
    check(focus.session() == 2, "second session");
    drawPomodoro(canvas, data, focus, t, "out/pomodoro-2-focus.pbm");

    // Pausing stops the countdown
    focus.pause(t, clockAt(t));
    focus.tick(t + 10 * MINUTE_MS);
    check(focus.minutesLeft() == 18, "pause stops the countdown");
    drawPomodoro(canvas, data, focus, t, "out/pomodoro-3-paused.pbm");

    // Resume and finish session 2
    t += 10 * MINUTE_MS;
    focus.resume(t, clockAt(t));
    t += 18 * MINUTE_MS;
    focus.tick(t);
    check(focus.state() == Pomodoro::FOCUS_DONE, "alert after focus");
    check(focus.todayFocusMinutes(t) == 50, "two sessions = 50 focus minutes");
    drawPomodoro(canvas, data, focus, t, "out/pomodoro-4-focus-done.pbm");

    // Break, 2 minutes in: 3 min left
    focus.start(t, clockAt(t));
    t += 2 * MINUTE_MS;
    focus.tick(t);
    check(focus.minutesLeft() == 3, "3 min of break left");
    drawPomodoro(canvas, data, focus, t, "out/pomodoro-5-break.pbm");

    // Break over: back to READY for session 3
    t += 3 * MINUTE_MS;
    focus.tick(t);
    check(focus.state() == Pomodoro::READY && focus.session() == 3, "ready for session 3");

    // All four sessions
    Pomodoro cycle;
    t = runSessions(cycle, Pomodoro::SESSIONS, 0);
    check(cycle.state() == Pomodoro::CYCLE_DONE, "cycle done after 4 sessions");
    check(cycle.cycleFocusMinutes() == 100, "4 x 25 = 100 min");
    drawPomodoro(canvas, data, cycle, t, "out/pomodoro-6-cycle-done.pbm");

    // Pause pressed just after the focus ended (during a slow refresh): the
    // alert shows, not a paused timer at 0 min that can't be stopped
    Pomodoro late;
    late.start(0, clockAt(0));
    t = late.focusMinutes() * MINUTE_MS + 2000;
    late.pause(t, clockAt(t));
    check(late.state() == Pomodoro::FOCUS_DONE, "pause after the end shows the break alert");

    if (checksFailed == failedBefore) {
        printf("Pomodoro checks passed\n");
    }
}

// ---------- Calendar ----------

// Events from the Figma design, written as an iCal feed so the parser is tested too:
// folded line, escaped comma, alarm with its own SUMMARY, cancelled and all-day events
static const char* const DEMO_ICS =
    "BEGIN:VCALENDAR\r\n"
    "BEGIN:VTIMEZONE\r\nTZID:Europe/Helsinki\r\nBEGIN:STANDARD\r\n"
    "DTSTART:19961027T040000\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Finnish class (X900BJ41-3015)/ IT24SP (Anna M\xC3\xA4ntykoski)\r\n"
    "DTSTART;TZID=Europe/Helsinki:20261005T090000\r\n"
    "DTEND;TZID=Europe/Helsinki:20261005T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Gym\r\nDTSTART:20261005T163000\r\nDTEND:20261005T173000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Project \r\n meeting\r\nDTSTART:20261005T103000\r\n"
    "DTEND:20261005T113000\r\nBEGIN:VALARM\r\nSUMMARY:Alarm text\r\nEND:VALARM\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Lunch with team\r\nDTSTART:20261005T121500\r\n"
    "DTEND:20261005T130000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:IoT lab session\r\nDTSTART:20261005T140000\r\n"
    "DTEND:20261005T160000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Cancelled talk\r\nSTATUS:CANCELLED\r\nDTSTART:20261006T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Finnish class\r\nDTSTART:20261007T090000\r\nDTEND:20261007T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Sync\\, weekly\r\nDTSTART:20261007T130000\r\nDTEND:20261007T133000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Finnish class\r\nDTSTART:20261009T090000\r\nDTEND:20261009T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Report\r\nDTSTART:20261009T150000\r\nDTEND:20261009T160000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Market\r\nDTSTART:20261010T110000\r\nDTEND:20261010T120000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Holiday\r\nDTSTART;VALUE=DATE:20261014\r\n"
    "DTEND;VALUE=DATE:20261015\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Seminar\r\nDTSTART:20261019T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Seminar\r\nDTSTART:20261021T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Seminar\r\nDTSTART:20261027T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Seminar\r\nDTSTART:20261028T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Seminar\r\nDTSTART:20261030T100000\r\nEND:VEVENT\r\n"
    "BEGIN:VEVENT\r\nSUMMARY:Too late\r\nDTSTART:20270301T100000\r\nEND:VEVENT\r\n"
    "END:VCALENDAR";

// Feeds an iCal file to the parser in small pieces like the network does.
// Returns the number of events, -1 if there was not enough memory.
static int parseIcs(const char* ics, int32_t firstDay, int32_t lastDay, CalendarEvent* events, int capacity,
                    int32_t& completeUntil) {
    std::unique_ptr<IcsParser> parser(IcsParser::create(firstDay, lastDay, events, capacity));
    if (!parser) {
        return -1;
    }
    size_t length = strlen(ics);
    for (size_t i = 0; i < length; i += 7) {
        parser->feed((const uint8_t*)ics + i, length - i < 7 ? length - i : 7);
    }
    parser->finish();
    check(parser->sawCalendar(), "the file is a calendar");
    completeUntil = parser->completeUntil();
    return parser->count();
}

// "20261005", for the generated test feeds
static std::string icsDate(int32_t day) {
    int year, month, dayOfMonth;
    DateUtil::civilFromDays(day, year, month, dayOfMonth);
    char text[12];
    snprintf(text, sizeof(text), "%04d%02d%02d", year, month, dayOfMonth);
    return text;
}

// Loads DEMO_ICS for the same days as updateCalendar() in main.cpp
static void loadDemoCalendar(AppData& data) {
    int32_t today = DateUtil::dayOf(data.now);
    int32_t firstDay = today - DateUtil::weekday(today);
    int32_t lastDay = today + CALENDAR_DAYS_AHEAD;
    int32_t completeUntil = 0;
    data.eventCount = parseIcs(DEMO_ICS, firstDay, lastDay, data.events, MAX_CALENDAR_EVENTS, completeUntil);
    data.calendarValid = true;
    data.calendarFirstDay = firstDay;
    data.calendarLastDay = completeUntil;

    int first;
    check(data.eventCount == 16, "cancelled and far away events are skipped");
    check(completeUntil == lastDay, "nothing left out of the demo feed");
    check(CalendarData::eventsOnDay(data, today, &first) == 5, "five events today");
    check(strcmp(data.events[0].title, "Finnish class") == 0, "title is cut before brackets");
    check(data.events[1].startMin == 10 * 60 + 30, "events are sorted by time");
    check(strcmp(data.events[1].title, "Project meeting") == 0, "folded line, alarm ignored");
    CalendarData::eventsOnDay(data, today + 2, &first);
    check(first >= 0 && strcmp(data.events[first + 1].title, "Sync, weekly") == 0, "escaped comma");
    CalendarData::eventsOnDay(data, today + 9, &first);
    check(first >= 0 && data.events[first].allDay, "all-day event");
    check(DateUtil::isoWeek(today) == 41, "5 Oct 2026 is in week 41");
    check(DateUtil::weekday(today) == 0, "5 Oct 2026 is a Monday");
}

// More recurring events or changes than the parser can keep: the days they
// touch must be unknown, not shown as free
static void checkParserLimits(int32_t today) {
    static CalendarEvent events[MAX_CALENDAR_EVENTS];
    int32_t lastDay = today + CALENDAR_DAYS_AHEAD;
    int32_t completeUntil = 0;
    char text[160];

    // One weekly series more than fits; the extra one starts next Monday
    std::string ics = "BEGIN:VCALENDAR\r\n";
    for (int i = 0; i <= IcsParser::MAX_RECURRING; i++) {
        int32_t start = i < IcsParser::MAX_RECURRING ? today : today + 7;
        snprintf(text, sizeof(text),
                 "BEGIN:VEVENT\r\nUID:series-%d\r\nSUMMARY:Series %d\r\nDTSTART:%sT%02d0000\r\n"
                 "RRULE:FREQ=WEEKLY;COUNT=2\r\nEND:VEVENT\r\n",
                 i, i, icsDate(start).c_str(), 8 + i % 10);
        ics += text;
    }
    ics += "END:VCALENDAR\r\n";
    int count = parseIcs(ics.c_str(), today, lastDay, events, MAX_CALENDAR_EVENTS, completeUntil);
    check(count == 2 * IcsParser::MAX_RECURRING, "every series that fits is expanded");
    check(completeUntil == today + 6, "a left-out series makes its days unknown");

    // Two daily series, with one cancelled occurrence more than fits
    const int perSeries = IcsParser::MAX_OVERRIDES / 2 + 1;  // 33 + 32 = MAX_OVERRIDES + 1
    ics = "BEGIN:VCALENDAR\r\n";
    for (int s = 0; s < 2; s++) {
        snprintf(text, sizeof(text),
                 "BEGIN:VEVENT\r\nUID:daily-%d\r\nSUMMARY:Daily %d\r\nDTSTART:%sT090000\r\n"
                 "RRULE:FREQ=DAILY;COUNT=40\r\nEND:VEVENT\r\n",
                 s, s, icsDate(today).c_str());
        ics += text;
    }
    for (int s = 0; s < 2; s++) {
        for (int d = 0; d < perSeries - s; d++) {
            snprintf(text, sizeof(text),
                     "BEGIN:VEVENT\r\nUID:daily-%d\r\nRECURRENCE-ID:%sT090000\r\nSTATUS:CANCELLED\r\n"
                     "DTSTART:%sT090000\r\nEND:VEVENT\r\n",
                     s, icsDate(today + d).c_str(), icsDate(today + d).c_str());
            ics += text;
        }
    }
    ics += "END:VCALENDAR\r\n";
    count = parseIcs(ics.c_str(), today, lastDay, events, MAX_CALENDAR_EVENTS, completeUntil);
    // The change left out is the last one: series 1 on day perSeries - 2
    check(count > 0 && events[0].day > today, "cancelled occurrences are hidden");
    check(completeUntil == today + (perSeries - 2) - 1, "a left-out change makes its day unknown");
}

static void drawCalendar(GFXcanvas1& canvas, const AppData& data, CalendarPage& page,
                         const char* path) {
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data);
    savePbm(canvas, path);
}

static void saveCalendarPages(GFXcanvas1& canvas, const AppData& source) {
    static AppData data;  // Large because of the event list
    data = source;
    int failedBefore = checksFailed;
    loadDemoCalendar(data);
    int32_t today = DateUtil::dayOf(data.now);
    checkParserLimits(today);

    CalendarPage page;
    page.open();
    drawCalendar(canvas, data, page, "out/calendar-1-month.pbm");

    page.move(data, 1);  // Tomorrow has no events
    drawCalendar(canvas, data, page, "out/calendar-2-free-day.pbm");

    page.open();
    page.toggleView();
    drawCalendar(canvas, data, page, "out/calendar-3-week.pbm");

    // Turning far ahead stops at the last loaded day, 30 Nov 2026:
    // November needs six rows of weeks
    page.open();
    page.move(data, 100);
    drawCalendar(canvas, data, page, "out/calendar-4-six-weeks.pbm");

    // The feed had too many events: the days after calendarLastDay are unknown
    page.open();
    page.move(data, 3);
    data.calendarLastDay = today + 1;
    drawCalendar(canvas, data, page, "out/calendar-5-cut.pbm");
    page.toggleView();
    drawCalendar(canvas, data, page, "out/calendar-6-week-cut.pbm");

    // Before the first download
    data.calendarValid = false;
    data.eventCount = 0;
    page.open();
    drawCalendar(canvas, data, page, "out/calendar-7-not-loaded.pbm");

    if (checksFailed == failedBefore) {
        printf("Calendar checks passed\n");
    }
}

// ---------- GitHub ----------

// An answer shaped like GitHub's GraphQL one: `days` days up to lastDay in
// weeks from Sunday. Weekdays are busier; the newest `streakDays` days all
// have contributions (today: 3, level 2), the day before them none.
static std::string demoGitHubAnswer(int32_t lastDay, int days, int streakDays) {
    static const char* const LEVELS[5] = {"NONE", "FIRST_QUARTILE", "SECOND_QUARTILE", "THIRD_QUARTILE",
                                          "FOURTH_QUARTILE"};
    std::string json = "{\"data\":{\"viewer\":{\"login\":\"octocat\",\"contributionsCollection\":"
                       "{\"contributionCalendar\":{\"totalContributions\":1234,\"weeks\":[";
    unsigned seed = 11;
    bool weekOpen = false;
    char text[128];
    for (int32_t day = lastDay - days + 1; day <= lastDay; day++) {
        int age = int(lastDay - day);
        int level;
        if (age < streakDays) {
            level = age == 0 ? 2 : 1 + age % 4;
        } else if (age == streakDays) {
            level = 0;
        } else {
            seed = seed * 1103515245 + 12345;
            int r = (seed >> 16) % 10;
            bool weekend = DateUtil::weekday(day) >= 5;
            level = weekend ? (r < 7 ? 0 : 1) : (r < 3 ? 0 : r < 5 ? 1 : r < 7 ? 2 : r < 9 ? 3 : 4);
        }
        int count = level == 0 ? 0 : age == 0 ? 3 : level * 2;

        if (!weekOpen) {
            json += "{\"contributionDays\":[";
            weekOpen = true;
        } else if (GitHubData::rowOf(day) == 0) {
            json += "]},{\"contributionDays\":[";  // A new week from Sunday
        } else {
            json += ",";
        }
        int year, month, dayOfMonth;
        DateUtil::civilFromDays(day, year, month, dayOfMonth);
        snprintf(text, sizeof(text),
                 "{\"contributionCount\":%d,\"contributionLevel\":\"%s\",\"date\":\"%04d-%02d-%02d\"}", count,
                 LEVELS[level], year, month, dayOfMonth);
        json += text;
    }
    json += "]}]}}}}}";
    return json;
}

// Reads a demo answer into data (for the widget pictures), checking the parser on the way
static void loadDemoGitHub(AppData& data) {
    int failedBefore = checksFailed;
    int32_t today = DateUtil::dayOf(data.now);
    JsonDocument doc;
    GitHubContributions& g = data.github;

    check(!deserializeJson(doc, demoGitHubAnswer(today, GITHUB_MAX_DAYS, 6)), "GitHub demo answer is JSON");
    check(readContributions(doc.as<JsonObjectConst>(), g), "GitHub answer is read");
    check(strcmp(g.login, "octocat") == 0 && g.total == 1234, "login and yearly total");
    check(g.lastDay == today && g.dayCount == GITHUB_MAX_DAYS, "every day placed, up to today");
    check(g.lastDayCount == 3 && GitHubData::levelOn(g, today) == 2, "today's count and level");
    check(GitHubData::levelOn(g, today + 1) == -1 && GitHubData::levelOn(g, today - GITHUB_MAX_DAYS) == -1,
          "days outside the data have no level");
    check(GitHubData::streak(g) == 6, "6-day streak");
    data.githubTokenSet = true;
    data.githubValid = true;
    check(GitHubData::today(data) == 3, "3 contributions today");

    // Nothing yet today doesn't end the streak, the day isn't over
    GitHubContributions noneToday = g;
    noneToday.levels[noneToday.dayCount - 1] = 0;
    check(GitHubData::streak(noneToday) == 5, "an empty today keeps yesterday's streak");

    // More days than fit: the newest are kept
    GitHubContributions longer;
    deserializeJson(doc, demoGitHubAnswer(today, GITHUB_MAX_DAYS + 9, 6));
    check(readContributions(doc.as<JsonObjectConst>(), longer) && longer.dayCount == GITHUB_MAX_DAYS &&
              longer.lastDay == today && longer.lastDayCount == 3 && GitHubData::streak(longer) == 6,
          "a longer answer keeps the newest days");

    // A refused token: GraphQL answers with errors and no data
    GitHubContributions refused;
    deserializeJson(doc, "{\"errors\":[{\"message\":\"Bad credentials\"}]}");
    check(!readContributions(doc.as<JsonObjectConst>(), refused), "an error answer is not read");

    if (checksFailed == failedBefore) {
        printf("GitHub checks passed\n");
    }
}

// ---------- Now playing ----------

// A message from the extension, as main.cpp's applyNowPlaying() stores it,
// received `ago` seconds before data.epoch
static void playSong(AppData& data, const char* json, long ago) {
    NowPlaying song;
    check(parseNowPlaying(json, song), "music message is read");
    data.musicValid = true;
    data.musicOpen = song.title[0] != '\0';
    data.musicPlaying = song.playing;
    strcpy(data.musicTitle, song.title);
    strcpy(data.musicArtist, song.artist);
    strcpy(data.musicAlbum, song.album);
    data.musicPosition = song.position;
    data.musicDuration = song.duration;
    data.musicUpdated = data.epoch - ago;
}

static void checkNowPlaying(AppData data) {
    int failedBefore = checksFailed;
    NowPlaying song;

    check(!parseNowPlaying("{\"artist\":\"x\"}", song), "a message without title or state is refused");
    check(!parseNowPlaying("not json", song), "not JSON is refused");
    check(parseNowPlaying("{\"title\":\"Beyonc\xC3\xA9\",\"playing\":false}", song) &&
              strcmp(song.title, "Beyonc?") == 0,
          "what the extension didn't make ASCII becomes '?'");
    check(parseNowPlaying("{\"title\":\"x\",\"playing\":true,\"position\":500,\"duration\":200}", song) &&
              song.position == 200,
          "the position is kept inside the song");

    playSong(data, "{\"title\":\"x\",\"playing\":true,\"position\":100,\"duration\":200}", 40);
    check(MusicData::active(data) && MusicData::position(data) == 140, "playing: the time counts on");
    data.musicPlaying = false;
    check(MusicData::position(data) == 100, "paused: the time stays");
    data.musicPlaying = true;
    data.musicUpdated = data.epoch - 59;
    check(MusicData::position(data) == 159 && MusicData::active(data), "still active just before the timeout");
    data.musicUpdated = data.epoch - MUSIC_STALE_S - 1;
    check(!MusicData::active(data) && MusicData::hasLast(data), "no message for a while: nothing plays");
    playSong(data, "{\"title\":\"x\",\"playing\":true,\"position\":190,\"duration\":200}", 50);
    check(MusicData::position(data) == 200, "the time stops at the end of the song");

    char text[16];
    MusicData::formatTime(text, sizeof(text), 3723);
    check(strcmp(text, "1:02:03") == 0, "hours in long times");

    if (checksFailed == failedBefore) {
        printf("Music checks passed\n");
    }
}

static void saveMusicPages(GFXcanvas1& canvas, const AppData& source) {
    static AppData data;  // Large because of the event list
    data = source;
    checkNowPlaying(data);
    MusicPage page;
    WidgetPage widgets;

    // Playing, 2:22 of 4:38 (sent 40 s ago at 1:42)
    playSong(data,
             "{\"title\":\"Fourth of July\",\"artist\":\"Sufjan Stevens\",\"album\":\"Carrie & Lowell\","
             "\"playing\":true,\"position\":102,\"duration\":278}",
             40);
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data);
    savePbm(canvas, "out/music.pbm");
    data.widgetCount = layoutWidgets("music:L,music:M,music:S,music:W", data.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgets.draw(canvas, data);
    savePbm(canvas, "out/home2-music.pbm");
    MenuPage menu;
    menu.select(MenuPage::MUSIC);
    canvas.fillScreen(COLOR_BG);
    menu.draw(canvas, data);
    savePbm(canvas, "out/menu-music.pbm");

    // Paused, with a title too long for the big font
    playSong(data,
             "{\"title\":\"Hey Jude - Remastered 2015 (Live at the Very Long Concert Hall)\","
             "\"artist\":\"The Beatles\",\"album\":\"\",\"playing\":false,\"position\":30,\"duration\":431}",
             5);
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data);
    savePbm(canvas, "out/music-paused.pbm");
    canvas.fillScreen(COLOR_BG);
    widgets.draw(canvas, data);
    savePbm(canvas, "out/home2-music-paused.pbm");

    // The browser was closed: nothing playing, the last song stays
    data.musicUpdated = data.epoch - 10 * 60;
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data);
    savePbm(canvas, "out/music-empty.pbm");
    canvas.fillScreen(COLOR_BG);
    widgets.draw(canvas, data);
    savePbm(canvas, "out/home2-music-empty.pbm");

    // The extension never sent anything
    data.musicValid = false;
    data.musicTitle[0] = '\0';
    canvas.fillScreen(COLOR_BG);
    page.draw(canvas, data);
    savePbm(canvas, "out/music-none.pbm");
}

// ---------- Habit tracker and Device pages ----------

// One habit's days, oldest first, the last one is today: "1101111" ('1' = done)
static void setHabitDays(Habits& habits, int habit, const char* days) {
    int n = int(strlen(days));
    for (int i = 0; i < n; i++) {
        int daysAgo = n - 1 - i;
        if (days[i] == '1') {
            habits.history[habit] |= uint64_t(1) << daysAgo;
        }
    }
}

// Same as the Figma design (Monday 5 Oct, last 7 days T W T F S S M)
static Habits demoHabits(int32_t today) {
    Habits habits;
    habits.rollTo(today);
    setHabitDays(habits, 0, "0111111111111111");  // Drink water, 15 days in a row
    setHabitDays(habits, 1, "1101111");           // Read 20 pages
    setHabitDays(habits, 2, "01111110");          // Finnish practice, not yet today
    setHabitDays(habits, 3, "0101100");           // Walk 30 min
    setHabitDays(habits, 4, "1011011");           // No phone after 22:00
    return habits;
}

static void checkHabits(int32_t today) {
    int failedBefore = checksFailed;
    Habits habits = demoHabits(today);

    check(habits.doneToday() == 3, "3 of 5 done today, like the design");
    check(habits.streak(0) == 15, "streak of 15 days");
    check(habits.streak(1) == 4, "a missed day ends the streak");
    check(habits.streak(2) == 6, "not done today yet: the streak up to yesterday still counts");
    check(habits.streak(3) == 0 && habits.streak(4) == 2, "no streak; short streak");

    habits.toggleToday(2);
    check(habits.done(2, 0) && habits.streak(2) == 7 && habits.doneToday() == 4, "check off today");
    habits.toggleToday(2);
    check(!habits.done(2, 0) && habits.streak(2) == 6, "press again: undo");

    check(!habits.rollTo(today) && !habits.rollTo(today - 3), "same day or clock back: nothing moves");
    check(habits.rollTo(today + 1) && habits.done(0, 1) && !habits.done(0, 0) && habits.streak(0) == 15,
          "next day: today's dots move to yesterday");
    check(habits.rollTo(today + 3) && habits.streak(0) == 0, "two days missed: the streak is over");
    habits.rollTo(today + 200);
    check(habits.streak(1) == 0 && !habits.done(1, 63), "after a long time everything is old");

    Habits fresh;
    check(!fresh.known(), "the date is unknown before the clock is synced");
    fresh.toggleToday(0);
    check(fresh.doneToday() == 0, "no check-off without the date");

    for (int i = 0; i < 70; i++) {
        habits.rollTo(today + 300 + i);
        habits.toggleToday(4);
    }
    check(habits.streak(4) == HABIT_HISTORY_DAYS, "the longest streak is the history length");

    if (checksFailed == failedBefore) {
        printf("Habit checks passed\n");
    }
}

static void saveHabitPages(GFXcanvas1& canvas, const AppData& source) {
    static AppData data;  // Large because of the event list
    data = source;
    data.now.tm_min = 35;  // 10:35 like the design

    int32_t today = DateUtil::dayOf(data.now);
    checkHabits(today);
    Habits habits = demoHabits(today);

    HabitPage habitPage;
    habitPage.next();
    habitPage.next();  // The arrow on "Finnish practice", like the design
    canvas.fillScreen(COLOR_BG);
    habitPage.draw(canvas, data, habits);
    savePbm(canvas, "out/habits.pbm");

    Habits unknown;
    canvas.fillScreen(COLOR_BG);
    habitPage.draw(canvas, data, unknown);
    savePbm(canvas, "out/habits-no-clock.pbm");

    MenuPage menu;
    menu.select(MenuPage::HABITS);
    menu.setHabits(habits.doneToday(), HABIT_COUNT, true);
    canvas.fillScreen(COLOR_BG);
    menu.draw(canvas, data);
    savePbm(canvas, "out/menu-habits.pbm");
}

// ---------- Weather answer ----------

static bool weatherFrom(const char* json, Weather& out) {
    JsonDocument doc;
    return !deserializeJson(doc, json) && readWeather(doc.as<JsonObjectConst>(), out);
}

static void checkWeather() {
    int failedBefore = checksFailed;
    Weather w;

    // Shortened real answer from /data/2.5/weather?units=metric
    check(weatherFrom("{\"weather\":[{\"id\":803,\"main\":\"Clouds\"}],\"main\":{\"temp\":-3.4,\"feels_like\":-8.9,"
                      "\"humidity\":86},\"wind\":{\"speed\":4.6,\"deg\":230},"
                      "\"sys\":{\"country\":\"FI\",\"sunrise\":1794812400,\"sunset\":1794837600},\"name\":\"Kuopio\"}",
                      w),
          "weather answer is read");
    check(w.code == 803 && fabsf(w.temperature + 3.4f) < 0.01f && fabsf(w.feelsLike + 8.9f) < 0.01f,
          "condition, temperature and feels like");
    check(fabsf(w.windSpeed - 4.6f) < 0.01f && w.windDeg == 230, "wind speed and direction");
    check(w.sunrise == 1794812400 && w.sunset == 1794837600, "sunrise and sunset");

    // Calm, polar night: no wind direction, no sunrise or sunset, no feels like
    check(weatherFrom("{\"weather\":[{\"id\":600}],\"main\":{\"temp\":-24},\"wind\":{\"speed\":0.3},\"sys\":{}}", w),
          "optional values may be missing");
    check(w.windDeg == -1 && w.sunrise == 0 && w.sunset == 0 && w.feelsLike == -24,
          "missing values: no direction, no sunrise, feels like = temperature");

    check(!weatherFrom("{\"cod\":401,\"message\":\"Invalid API key\"}", w), "an error answer is not read");
    check(!weatherFrom("{\"weather\":[],\"main\":{\"temp\":5}}", w), "no condition code: not read");
    check(!weatherFrom("{\"weather\":[{\"id\":800}],\"main\":{}}", w), "no temperature: not read");
    check(!weatherFrom("{\"weather\":[{\"id\":800}],\"main\":{\"temp\":\"warm\"}}", w), "a text temperature: not read");

    // Redraw only when something on the screen changes
    static AppData data;
    data = AppData();
    Weather now;
    now.temperature = 20.3f;
    now.code = 800;
    now.feelsLike = 19.6f;
    now.windSpeed = 3.2f;
    now.windDeg = 90;
    now.sunrise = 1794812400;
    now.sunset = 1794837600;
    check(storeWeather(now, data) && data.weatherValid && data.outdoorTemp == 20.3f, "the first weather redraws");
    check(!storeWeather(now, data), "the same weather doesn't redraw");
    Weather next = now;
    next.temperature = 20.4f;
    next.windSpeed = 3.4f;
    next.sunrise += 30;  // Same minute
    check(!storeWeather(next, data), "changes too small to show don't redraw");
    check(data.outdoorTemp == 20.4f, "... but the new values are kept");
    next.temperature = 20.6f;
    check(storeWeather(next, data), "21 instead of 20 degrees redraws");
    next.windDeg = 100;
    check(storeWeather(next, data), "a new wind direction redraws");
    next.code = 500;
    check(storeWeather(next, data), "rain instead of clear sky redraws");
    next.sunset += 60;
    check(storeWeather(next, data), "sunset a minute later redraws");

    if (checksFailed == failedBefore) {
        printf("Weather checks passed\n");
    }
}

// ---------- Download schedule ----------

// Local time on 16 Nov 2026 (Finnish time, see main())
static time_t localTime(int dayOffset, int hour, int minute) {
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 10;
    t.tm_mday = 16 + dayOffset;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_isdst = -1;
    return mktime(&t);
}

// Prices data at `hour` today, with quarter-hours for `days` days from today (or from yesterday)
static void pricesAt(AppData& data, int hour, int firstDay, int days) {
    time_t moment = localTime(0, hour, 0);
    data.epoch = moment;
    localtime_r(&moment, &data.now);
    data.priceCount = 0;
    for (int d = 0; d < days; d++) {
        for (int q = 0; q < 96; q++) {
            data.prices[data.priceCount++].start = localTime(firstDay + d, 0, 0) + q * 15 * 60;
        }
    }
    data.pricesValid = data.priceCount > 0;
}

static void checkSchedule() {
    namespace Fetch = FetchSchedule;  // Its MINUTE_MS is the same as this file's
    int failedBefore = checksFailed;

    Fetch::Timer timer;
    check(timer.due(0, 15 * MINUTE_MS, false), "the first download starts at once");
    check(!timer.due(14 * MINUTE_MS, 15 * MINUTE_MS, false), "not again before the interval");
    check(timer.due(15 * MINUTE_MS, 15 * MINUTE_MS, false), "again after the interval");
    check(timer.due(15 * MINUTE_MS + 1, 15 * MINUTE_MS, true), "refresh skips the interval");

    // millis() goes back to 0 after 49.7 days
    Fetch::Timer wrap;
    uint32_t start = 0xFFFFFFFFu - MINUTE_MS;
    wrap.due(start, 15 * MINUTE_MS, false);
    check(!wrap.due(start + 14 * MINUTE_MS, 15 * MINUTE_MS, false), "the interval holds when millis() wraps");
    check(wrap.due(start + 15 * MINUTE_MS, 15 * MINUTE_MS, false), "and ends on time");

    // Prices: refresh never asks more often than every 2 minutes
    Fetch::Timer prices;
    prices.due(0, 6 * 60 * MINUTE_MS, false, Fetch::PRICES_MIN_GAP_MS);
    check(!prices.due(MINUTE_MS, 6 * 60 * MINUTE_MS, true, Fetch::PRICES_MIN_GAP_MS), "price refresh waits 2 minutes");
    check(prices.due(Fetch::PRICES_MIN_GAP_MS, 6 * 60 * MINUTE_MS, true, Fetch::PRICES_MIN_GAP_MS), "then refresh works");

    static AppData data;
    data = AppData();
    check(Fetch::weatherInterval(data) == MINUTE_MS && Fetch::calendarInterval(data) == 2 * MINUTE_MS &&
              Fetch::githubInterval(data) == 5 * MINUTE_MS,
          "before the first answer: try again soon");
    data.weatherValid = data.calendarValid = data.githubValid = true;
    check(Fetch::weatherInterval(data) == 15 * MINUTE_MS && Fetch::calendarInterval(data) == 30 * MINUTE_MS &&
              Fetch::githubInterval(data) == 30 * MINUTE_MS,
          "after that: weather 15 min, calendar and GitHub 30 min");

    pricesAt(data, 10, 0, 0);
    check(Fetch::pricesInterval(data) == 5 * MINUTE_MS, "no prices: every 5 min");
    pricesAt(data, 10, -1, 1);
    check(Fetch::pricesInterval(data) == 5 * MINUTE_MS, "only yesterday's prices: every 5 min");
    pricesAt(data, 10, 0, 1);
    check(Fetch::pricesInterval(data) == 6 * 60 * MINUTE_MS, "today's prices in the morning: every 6 h");
    pricesAt(data, 14, 0, 1);
    check(Fetch::pricesInterval(data) == 30 * MINUTE_MS, "from 14:00 until tomorrow's are out: every 30 min");
    pricesAt(data, 15, 0, 2);
    check(Fetch::pricesInterval(data) == 6 * 60 * MINUTE_MS, "tomorrow's prices are there: every 6 h");

    if (checksFailed == failedBefore) {
        printf("Schedule checks passed\n");
    }
}

// ---------- Buttons ----------

static void checkButtons() {
    int failedBefore = checksFailed;

    PressGesture tap(3000);
    check(tap.update(false, 0) == PressGesture::NONE, "nothing happens without a touch");
    check(tap.update(true, 100) == PressGesture::DOWN && tap.isDown(), "touch starts");
    check(tap.update(true, 400) == PressGesture::NONE, "a short touch is no hold yet");
    check(tap.update(false, 500) == PressGesture::TAP && !tap.isDown(), "letting go is a tap");

    PressGesture hold(3000);
    hold.update(true, 1000);
    check(hold.update(true, 3999) == PressGesture::NONE, "no hold before 3 s");
    check(hold.update(true, 4000) == PressGesture::HOLD, "hold after 3 s, while still held");
    check(hold.update(true, 9000) == PressGesture::NONE, "the hold fires once");
    check(hold.update(false, 9100) == PressGesture::NONE, "letting go after a hold is no tap");
    check(hold.update(true, 9500) == PressGesture::DOWN && hold.update(false, 9600) == PressGesture::TAP,
          "the next touch is a tap again");

    // The loop was busy (E-Ink refresh): only the interrupt saw the touch, the pin is LOW already
    PressGesture missed(3000);
    missed.update(true, 20000);
    check(missed.update(false, 20000) == PressGesture::TAP, "a touch seen only by the interrupt is a tap");

    PressGesture wrap(800);
    wrap.update(true, 0xFFFFFFFFu - 100);
    check(wrap.update(true, 699) == PressGesture::HOLD, "hold works when millis() wraps");

    if (checksFailed == failedBefore) {
        printf("Button checks passed\n");
    }
}

// ---------- Widget layout ----------

static bool slotIs(const WidgetSlot& s, uint8_t type, uint8_t col, uint8_t row, uint8_t cols, uint8_t rows) {
    return s.type == type && s.col == col && s.row == row && s.cols == cols && s.rows == rows;
}

// Every widget inside the grid, in its size, none on top of another; `cells` = cells used
static bool fitsGrid(const WidgetSlot* slots, int count, int& cells) {
    bool used[WIDGET_ROWS][WIDGET_COLS] = {};
    cells = 0;
    for (int i = 0; i < count; i++) {
        const WidgetSlot& s = slots[i];
        if (s.cols != sizeCols(s.size) || s.rows != sizeRows(s.size) || s.col + s.cols > WIDGET_COLS ||
            s.row + s.rows > WIDGET_ROWS) {
            return false;
        }
        for (int r = s.row; r < s.row + s.rows; r++) {
            for (int c = s.col; c < s.col + s.cols; c++) {
                if (used[r][c]) {
                    return false;
                }
                used[r][c] = true;
                cells++;
            }
        }
    }
    return true;
}

static void checkWidgetLayout() {
    int failedBefore = checksFailed;
    WidgetSlot slots[MAX_WIDGETS];
    char text[200];

    int count = layoutWidgets("clock:S,weather:L,date:M", slots, MAX_WIDGETS);
    check(count == 3 && slotIs(slots[0], WIDGET_CLOCK, 0, 0, 1, 1) && slotIs(slots[1], WIDGET_WEATHER, 1, 0, 2, 2) &&
              slotIs(slots[2], WIDGET_DATE, 0, 2, 2, 1),
          "each widget goes to the first free spot, row by row");

    count = layoutWidgets("agenda:L,weather:W,clock:S,date:S,co2:S", slots, MAX_WIDGETS);
    check(count == 5 && slotIs(slots[1], WIDGET_WEATHER, 0, 2, 4, 1) && slotIs(slots[2], WIDGET_CLOCK, 2, 0, 1, 1) &&
              slotIs(slots[4], WIDGET_CO2, 2, 1, 1, 1),
          "small widgets fill the gaps a big one left");

    count = layoutWidgets("price:W,price:W,price:W,clock:S", slots, MAX_WIDGETS);
    check(count == 3, "a full grid leaves the rest out");
    count = layoutWidgets("agenda:L,claude:L,next:L,clock:S", slots, MAX_WIDGETS);
    check(count == 3 && slotIs(slots[2], WIDGET_CLOCK, 0, 2, 1, 1), "a widget that doesn't fit is skipped, the next one still goes in");

    count = layoutWidgets("clock:S,radio:M,date:X,date:,:S,weather,co2:s", slots, MAX_WIDGETS);
    check(count == 1 && slots[0].type == WIDGET_CLOCK, "unknown widgets and sizes are skipped");
    check(layoutWidgets("", slots, MAX_WIDGETS) == 0 && layoutWidgets(nullptr, slots, MAX_WIDGETS) == 0,
          "an empty layout has no widgets");
    check(layoutWidgets("clock:S,date:S,co2:S", slots, 2) == 2, "no more widgets than there is room for");

    std::string many;
    for (int i = 0; i < 14; i++) {
        many += i > 0 ? ",co2:S" : "co2:S";
    }
    check(layoutWidgets(many.c_str(), slots, MAX_WIDGETS) == MAX_WIDGETS, "at most 12 widgets");

    // Random layouts: always inside the grid without overlaps, and a 1 x 1
    // widget at the end is only left out when no cell is free
    unsigned seed = 5;
    bool allFit = true;
    bool smallPlaced = true;
    for (int round = 0; round < 3000; round++) {
        std::string layout;
        seed = seed * 1103515245 + 12345;
        int entries = (seed >> 16) % (MAX_WIDGETS - 1);
        for (int i = 0; i < entries; i++) {
            seed = seed * 1103515245 + 12345;
            uint8_t type = (seed >> 16) % WIDGET_TYPE_COUNT;
            uint8_t size = (seed >> 24) % SIZE_COUNT;
            layout += std::string(i > 0 ? "," : "") + widgetCode(type) + ":" + sizeCode(size);
        }
        int cells;
        count = layoutWidgets(layout.c_str(), slots, MAX_WIDGETS);
        allFit &= fitsGrid(slots, count, cells);
        int withSmall = layoutWidgets((layout + (entries > 0 ? "," : "") + "clock:S").c_str(), slots, MAX_WIDGETS);
        smallPlaced &= withSmall == count + (cells < WIDGET_COLS * WIDGET_ROWS ? 1 : 0);
    }
    check(allFit, "random layouts stay inside the grid without overlaps");
    check(smallPlaced, "a 1 x 1 widget always finds a free cell");

    check(normalizeWidgets("clock:S,radio:M,weather:L,date:Q", text, sizeof(text)) &&
              strcmp(text, "clock:S,weather:L") == 0,
          "cleaning keeps only known widgets and sizes");
    check(!normalizeWidgets("radio:M", text, sizeof(text)) && text[0] == '\0', "nothing valid: empty and false");
    check(normalizeWidgets(many.c_str(), text, sizeof(text)) && strlen(text) == 12 * 6 - 1, "cleaning keeps 12 entries");
    check(normalizeWidgets("clock:S,weather:L", text, 10) && strcmp(text, "clock:S") == 0,
          "a short buffer gets whole entries only");
    check(layoutWidgets(DEFAULT_WIDGETS, slots, MAX_WIDGETS) == 7, "the default layout fits");

    for (uint8_t t = 0; t < WIDGET_TYPE_COUNT; t++) {
        snprintf(text, sizeof(text), "%s:W", widgetCode(t));
        count = layoutWidgets(text, slots, MAX_WIDGETS);
        if (count != 1 || slots[0].type != t || widgetName(t)[0] == '\0') {
            check(false, "every widget has a code and a name");
        }
    }

    if (checksFailed == failedBefore) {
        printf("Widget layout checks passed\n");
    }
}

// ---------- Electricity prices ----------

static time_t localMidnight(int year, int month, int day) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_isdst = -1;
    return mktime(&t);
}

// Cents = the local hour; on the October change day the repeated hour costs 10, then 20
static float hourPrice(time_t moment) {
    struct tm local;
    localtime_r(&moment, &local);
    if (local.tm_mon == 9 && local.tm_hour == 3) {
        return local.tm_isdst > 0 ? 10 : 20;
    }
    return float(local.tm_hour);
}

static float flatPrice(time_t) {
    return 5;
}

// Every quarter-hour of one local day, `skipHour` left out (-1 = none); returns the count
static int fillPrices(AppData& data, int year, int month, int day, float (*cents)(time_t), int skipHour = -1) {
    data.priceCount = 0;
    time_t end = localMidnight(year, month, day + 1);
    for (time_t t = localMidnight(year, month, day); t < end && data.priceCount < MAX_PRICES; t += 15 * 60) {
        if (DateUtil::localMinute(t) / 60 == skipHour) {
            continue;
        }
        data.prices[data.priceCount].start = t;
        data.prices[data.priceCount].cents = cents(t);
        data.priceCount++;
    }
    data.pricesValid = true;
    return data.priceCount;
}

static void checkElectricity() {
    using namespace ElectricityData;
    int failedBefore = checksFailed;
    static AppData data;
    data = AppData();
    int32_t day = DateUtil::daysFromCivil(2026, 11, 16);
    char text[16];

    check(fillPrices(data, 2026, 11, 16, hourPrice) == 96, "a normal day has 96 quarter-hours");
    DayPrices d = dayPrices(data, day);
    check(d.any && d.hourly[5] == 5 && d.minimum == 0 && d.maximum == 23 && fabsf(d.average - 11.5f) < 0.01f,
          "hourly prices, minimum, maximum and average");
    check(!dayPrices(data, day + 1).any, "a day without prices has none");
    check(hasDay(data, day) && !hasDay(data, day + 1), "which days have prices");

    check(cheapestHours(d, 0, 3) == 0 && cheapestHours(d, 10, 3) == 10, "the cheapest 3 hours from a given hour");
    check(cheapestHours(d, 21, 3) == 21 && cheapestHours(d, 22, 3) == -1, "a run must end by midnight");
    fillPrices(data, 2026, 11, 16, flatPrice);
    check(cheapestHours(dayPrices(data, day), 5, 3) == 5, "equal prices: the earliest run");
    fillPrices(data, 2026, 11, 16, hourPrice, 2);
    check(cheapestHours(dayPrices(data, day), 0, 3) == 3, "a run never covers an hour without prices");

    // Summer time starts (03:00 -> 04:00) and ends (04:00 -> 03:00 again)
    check(fillPrices(data, 2026, 3, 29, hourPrice) == 92, "the spring change day has 92 quarter-hours");
    d = dayPrices(data, DateUtil::daysFromCivil(2026, 3, 29));
    check(!d.hasHour[3] && d.hasHour[2] && d.hasHour[4], "the skipped hour has no price");
    check(fillPrices(data, 2026, 10, 25, hourPrice) == 100, "the autumn change day has 100 quarter-hours");
    d = dayPrices(data, DateUtil::daysFromCivil(2026, 10, 25));
    check(d.hourly[3] == 15 && d.hourly[4] == 4, "the repeated hour is the average of both");

    fillPrices(data, 2026, 11, 16, hourPrice);
    time_t slot = data.prices[5].start;
    check(priceAt(data, slot) == &data.prices[5] && priceAt(data, slot + 899) == &data.prices[5] &&
              priceAt(data, slot + 900) == &data.prices[6],
          "each price holds for its 15 minutes");
    check(priceAt(data, data.prices[0].start - 1) == nullptr, "no price before the first one");
    data.epoch = slot;
    check(priceNow(data) == &data.prices[5], "the price now");
    data.epoch = 0;
    check(priceNow(data) == nullptr, "no price now without the time");

    check(strcmp(levelText(7.5f, 10), "Cheap now") == 0 && strcmp(levelText(7.6f, 10), "Normal price") == 0 &&
              strcmp(levelText(12.5f, 10), "Expensive now") == 0,
          "cheap or expensive: 25 % from the average");
    check(strcmp(levelText(-0.2f, 0.4f), "Cheap now") == 0 && strcmp(levelText(0.5f, 0.4f), "Normal price") == 0,
          "near zero the margin is at least 0.5 c");
    check(strcmp(levelText(-3, -2), "Cheap now") == 0, "negative prices");

    formatCents(text, sizeof(text), -0.04f);
    check(strcmp(text, "0.0") == 0, "no -0.0");
    formatCents(text, sizeof(text), -0.3f);
    check(strcmp(text, "-0.3") == 0, "negative cents");
    formatHours(text, sizeof(text), 22, 3);
    check(strcmp(text, "22:00-01:00") == 0, "hours past midnight");

    if (checksFailed == failedBefore) {
        printf("Electricity checks passed\n");
    }
}

// ---------- Claude usage message ----------

static void checkClaudeUsage() {
    int failedBefore = checksFailed;
    ClaudeUsage u;
    const time_t nov4 = time_t(DateUtil::daysFromCivil(2026, 11, 4)) * 86400;
    const time_t nov6 = time_t(DateUtil::daysFromCivil(2026, 11, 6)) * 86400;

    check(parseClaudeUsage("{\"session\":54.5,\"session_resets\":\"2026-11-04T17:30:00.294921+00:00\","
                           "\"weekly\":29,\"weekly_resets\":\"2026-11-06T22:00:00+02:00\"}",
                           u),
          "Claude usage message is read");
    check(u.sessionPercent == 54.5f && u.weeklyPercent == 29, "session and weekly percent");
    check(u.sessionResets == nov4 + 17 * 3600 + 30 * 60, "reset time, fractions of a second dropped");
    check(u.weeklyResets == nov6 + 20 * 3600, "reset time with a time zone");

    check(parseClaudeUsage("{\"session\":0,\"weekly\":100}", u) && u.sessionResets == 0 && u.weeklyResets == 0,
          "no reset times: unknown");
    check(parseClaudeUsage("{\"session\":1,\"weekly\":2,\"session_resets\":\"tomorrow\"}", u) && u.sessionResets == 0,
          "a bad reset time is unknown, the numbers still count");
    check(!parseClaudeUsage("{\"session\":54}", u), "no weekly number: refused");
    check(!parseClaudeUsage("{\"session\":\"54\",\"weekly\":29}", u), "a number as text: refused");
    check(!parseClaudeUsage("{}", u) && !parseClaudeUsage("not json", u) && !parseClaudeUsage("", u),
          "empty or not JSON: refused");

    if (checksFailed == failedBefore) {
        printf("Claude usage checks passed\n");
    }
}

// ---------- Dates ----------

static void checkDates() {
    using namespace DateUtil;
    int failedBefore = checksFailed;

    // Every day from 1900 to 2100: day numbers go up by one and convert back
    bool continuous = true;
    int32_t expected = daysFromCivil(1900, 1, 1);
    for (int year = 1900; year <= 2100; year++) {
        for (int month = 1; month <= 12; month++) {
            for (int day = 1; day <= daysInMonth(year, month); day++) {
                int y, m, dd;
                civilFromDays(expected, y, m, dd);
                continuous &= daysFromCivil(year, month, day) == expected && y == year && m == month && dd == day;
                expected++;
            }
        }
    }
    check(continuous, "day numbers are continuous and convert back (1900-2100)");
    check(daysFromCivil(1970, 1, 1) == 0 && weekday(0) == 3 && weekday(-1) == 2,
          "1 Jan 1970 is day 0, a Thursday; the day before a Wednesday");

    check(daysInMonth(2028, 2) == 29 && daysInMonth(2026, 2) == 28 && daysInMonth(2000, 2) == 29 &&
              daysInMonth(2100, 2) == 28,
          "leap years");
    check(isoWeek(daysFromCivil(2026, 12, 31)) == 53 && isoWeek(daysFromCivil(2027, 1, 1)) == 53 &&
              isoWeek(daysFromCivil(2027, 1, 4)) == 1,
          "2026 has 53 weeks, 1 Jan 2027 is still in week 53");
    check(isoWeek(daysFromCivil(2024, 12, 30)) == 1, "30 Dec 2024 is in week 1 of 2025");

    const time_t nov4 = time_t(daysFromCivil(2026, 11, 4)) * 86400;
    check(parseIsoTime("2026-11-04T17:30:00Z") == nov4 + 17 * 3600 + 30 * 60 &&
              parseIsoTime("2026-11-04T17:30:00") == nov4 + 17 * 3600 + 30 * 60,
          "UTC with Z or without a zone");
    check(parseIsoTime("2026-11-04T12:30:00-05:00") == nov4 + 17 * 3600 + 30 * 60, "a time zone behind UTC");
    check(parseIsoTime("2026-11-04") == 0 && parseIsoTime("2026-1x-04T17:30:00Z") == 0 && parseIsoTime(nullptr) == 0 &&
              parseIsoTime("2026-11-04T17:30:00+0200") == 0,
          "invalid times are 0");

    // 03:30 comes twice on 25 Oct 2026: at 00:30 and 01:30 UTC
    time_t first = time_t(daysFromCivil(2026, 10, 25)) * 86400 + 30 * 60;
    check(localMinute(first) == 3 * 60 + 30 && localMinute(first + 3600) == 3 * 60 + 30 &&
              localDay(first) == daysFromCivil(2026, 10, 25),
          "local time when summer time ends");

    if (checksFailed == failedBefore) {
        printf("Date checks passed\n");
    }
}

int main() {
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);  // Finnish time, like the device
    tzset();

    checkWeather();
    checkSchedule();
    checkButtons();
    checkWidgetLayout();
    checkElectricity();
    checkClaudeUsage();
    checkDates();

    GFXcanvas1 canvas(SCREEN_W, SCREEN_H);
    AppData data = demoData();

    HomePage home;
    canvas.fillScreen(COLOR_BG);
    home.draw(canvas, data);
    savePbm(canvas, "out/home.pbm");

    // Before Wi-Fi, NTP and sensors have delivered anything
    AppData empty;
    canvas.fillScreen(COLOR_BG);
    home.draw(canvas, empty);
    savePbm(canvas, "out/home-empty.pbm");

    MenuPage menu;
    menu.move(1);
    canvas.fillScreen(COLOR_BG);
    menu.draw(canvas, data);
    savePbm(canvas, "out/menu.pbm");

    SleepPage sleep;
    AppData night = data;
    night.now.tm_hour = 22;
    night.now.tm_min = 47;
    canvas.fillScreen(COLOR_BG);
    sleep.draw(canvas, night);
    savePbm(canvas, "out/sleep.pbm");

    // Turned off (touch button held on Home), and without a synced clock
    PowerOffPage off;
    canvas.fillScreen(COLOR_BG);
    off.draw(canvas, &night.now);
    savePbm(canvas, "out/power-off.pbm");
    canvas.fillScreen(COLOR_BG);
    off.draw(canvas, nullptr);
    savePbm(canvas, "out/power-off-no-time.pbm");

    savePomodoroPages(canvas, data);
    saveCalendarPages(canvas, data);
    saveMusicPages(canvas, data);
    saveHabitPages(canvas, data);

    // 10:35, for the widget and electricity pages below
    AppData calendarData = data;
    calendarData.now.tm_min = 35;

    // Setup page. The QR code here is only a look-alike: the real one is
    // made on the device (esp_qrcode), which doesn't build on a PC.
    AppData setup;
    setup.setupActive = true;
    strcpy(setup.setupNetwork, "Desk-Setup-4F2A");
    strcpy(setup.setupPassword, "k7mq2xpa");
    strcpy(setup.setupStatus, "Waiting for a phone to connect...");
    const int qr = 29;
    setup.qrSize = qr;
    unsigned seed = 7;
    for (int y = 0; y < qr; y++) {
        for (int x = 0; x < qr; x++) {
            bool dark;
            int fx = x < 7 ? x : x >= qr - 7 ? x - (qr - 7) : -1;
            int fy = y < 7 ? y : y >= qr - 7 ? y - (qr - 7) : -1;
            if (fx >= 0 && fy >= 0 && !(x >= qr - 7 && y >= qr - 7)) {
                // Finder pattern: ring and 3x3 center
                dark = fx == 0 || fx == 6 || fy == 0 || fy == 6 || (fx >= 2 && fx <= 4 && fy >= 2 && fy <= 4);
            } else {
                seed = seed * 1103515245 + 12345;
                dark = (seed >> 16) & 1;
            }
            if (dark) {
                int i = y * qr + x;
                setup.qrCode[i / 8] |= 0x80 >> (i % 8);
            }
        }
    }
    SetupPage setupPage;
    canvas.fillScreen(COLOR_BG);
    setupPage.draw(canvas, setup);
    savePbm(canvas, "out/setup.pbm");

    // Settings page, with the same look-alike QR code
    AppData settingsData = data;
    strcpy(settingsData.deviceIp, "192.168.1.23");
    strcpy(settingsData.wifiName, "DNA-WIFI-8C21");
    settingsData.qrSize = setup.qrSize;
    memcpy(settingsData.qrCode, setup.qrCode, sizeof(settingsData.qrCode));
    SettingsPage settingsPage;
    canvas.fillScreen(COLOR_BG);
    settingsPage.draw(canvas, settingsData);
    savePbm(canvas, "out/settings.pbm");

    // Second home page: the default layout, and one with every widget
    WidgetPage widgetPage;
    AppData widgetData = calendarData;
    widgetData.widgetCount = layoutWidgets(DEFAULT_WIDGETS, widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2.pbm");

    widgetData.widgetCount =
        layoutWidgets("agenda:L,weather:M,date:S,co2:S,price:W", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-all.pbm");

    widgetData.widgetCount = layoutWidgets("indoor:W,next:M,weather:S,date:S,clock:M,indoor:M", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-wide.pbm");

    widgetData.widgetCount = layoutWidgets("claude:L,claude:M,claude:S,clock:S,price:M,indoor:S,co2:S", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-claude.pbm");

    // Electricity page: today (at 10:35) and tomorrow
    AppData priceData = calendarData;
    ElectricityPage electricity;
    electricity.open();
    canvas.fillScreen(COLOR_BG);
    electricity.draw(canvas, priceData);
    savePbm(canvas, "out/electricity.pbm");
    electricity.toggleDay();
    canvas.fillScreen(COLOR_BG);
    electricity.draw(canvas, priceData);
    savePbm(canvas, "out/electricity-tomorrow.pbm");

    // Electricity widget in every size; and the menu with the price
    widgetData.widgetCount = layoutWidgets("price:L,price:M,price:S,date:S,clock:W", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-price.pbm");
    widgetData.widgetCount = layoutWidgets("price:W,claude:M,co2:S,date:S,next:M,clock:M", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-price-wide.pbm");

    // GitHub in every size; then without a token, and loading
    static AppData githubData;  // Large because of the event list
    githubData = widgetData;
    loadDemoGitHub(githubData);
    githubData.widgetCount = layoutWidgets("github:W,github:L,github:M,github:S", githubData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, githubData);
    savePbm(canvas, "out/home2-github.pbm");
    githubData.widgetCount = layoutWidgets("github:M,github:S,github:S,github:W", githubData.widgets, MAX_WIDGETS);
    githubData.githubTokenSet = false;
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, githubData);
    savePbm(canvas, "out/home2-github-none.pbm");
    githubData.githubTokenSet = true;
    githubData.githubValid = false;
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, githubData);
    savePbm(canvas, "out/home2-github-loading.pbm");
    githubData.githubTokenRefused = true;
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, githubData);
    savePbm(canvas, "out/home2-github-refused.pbm");

    // Weather in every size: large and wide add feels like, wind, sunrise, sunset
    AppData weatherData = widgetData;
    weatherData.widgetCount = layoutWidgets("weather:L,weather:M,weather:S,weather:W", weatherData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, weatherData);
    savePbm(canvas, "out/home2-weather.pbm");

    // Calm, polar night (no sunrise or sunset) and a long city name; then no weather yet
    weatherData.windSpeed = 0.3f;
    weatherData.windDeg = -1;
    weatherData.sunrise = weatherData.sunset = 0;
    weatherData.outdoorTemp = -24;
    weatherData.feelsLike = -31;
    strcpy(weatherData.city, "Kilpisjarvi, Enontekio");
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, weatherData);
    savePbm(canvas, "out/home2-weather-polar.pbm");
    weatherData.weatherValid = false;
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, weatherData);
    savePbm(canvas, "out/home2-weather-none.pbm");

    // Old numbers and no numbers yet
    widgetData.claudeUpdated = widgetData.epoch - 95 * 60;
    widgetData.widgetCount = layoutWidgets("claude:W,claude:M,claude:S,clock:S", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    widgetData.claudeValid = false;
    savePbm(canvas, "out/home2-claude-old.pbm");
    widgetData.widgetCount = layoutWidgets("claude:W,claude:M,claude:S", widgetData.widgets, MAX_WIDGETS);
    canvas.fillScreen(COLOR_BG);
    widgetPage.draw(canvas, widgetData);
    savePbm(canvas, "out/home2-claude-none.pbm");

    printf("Saved all pages (.pbm) in out/\n");
    return checksFailed > 0 ? 1 : 0;
}
