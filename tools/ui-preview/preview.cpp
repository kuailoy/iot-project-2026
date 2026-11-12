// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

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
#include "pages/CalendarPage.h"
#include "pages/ElectricityPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/PomodoroPage.h"
#include "pages/SettingsPage.h"
#include "pages/SetupPage.h"
#include "pages/WidgetPage.h"
#include "pages/SleepPage.h"
#include "../../src/services/IcsParser.h"

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

int main() {
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);  // Finnish time, like the device
    tzset();

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

    savePomodoroPages(canvas, data);
    saveCalendarPages(canvas, data);

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
