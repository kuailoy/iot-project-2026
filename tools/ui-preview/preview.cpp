// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

#include <cstdio>
#include <cstring>

#include <Adafruit_GFX.h>

#include "Components.h"
#include "pages/CalendarPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/PomodoroPage.h"
#include "pages/SleepPage.h"
#include "../../src/services/IcalParser.h"

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
    data.now.tm_hour = 10;
    data.now.tm_min = 22;
    data.now.tm_mday = 5;
    data.now.tm_mon = 9;   // October (0 = January)
    data.now.tm_wday = 1;  // Monday
    data.now.tm_yday = 277;
    data.now.tm_year = 126;  // 2026

    data.weatherValid = true;
    data.outdoorTemp = 8;

    data.sensorValid = true;
    data.indoorTemp = 22;
    data.humidity = 45;

    data.eventValid = true;
    strcpy(data.eventTime, "10:30");
    strcpy(data.eventTitle, "Project meeting");

    data.wifiConnected = true;
    return data;
}

// ---------- Pomodoro ----------

static const uint32_t MINUTE_MS = 60000UL;
static const int32_t START_CLOCK = 10 * 3600 + 23 * 60;  // First session starts 10:23

static int checksFailed = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        printf("Pomodoro check FAILED: %s\n", what);
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

    if (checksFailed == 0) {
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

static void loadDemoCalendar(AppData& data) {
    int32_t today = CalendarDate::fromTm(data.now);
    IcalParser parser(data.calendar, today - 7, today + CalendarData::DAYS_AHEAD);

    // Feed in small pieces like the network does
    size_t length = strlen(DEMO_ICS);
    for (size_t i = 0; i < length; i += 7) {
        parser.feed(DEMO_ICS + i, length - i < 7 ? length - i : 7);
    }
    parser.finish();
    data.calendar.valid = true;

    const CalendarData& calendar = data.calendar;
    check(parser.eventsSeen() == 18, "parser sees every event");
    check(calendar.count == 16, "cancelled and far away events are skipped");
    check(calendar.countOn(today) == 5, "five events today");
    check(strcmp(calendar.events[0].title, "Finnish class") == 0, "title is cut before brackets");
    check(calendar.events[1].startMinute == 10 * 60 + 30, "events are sorted by time");
    check(strcmp(calendar.events[1].title, "Project meeting") == 0, "folded line, alarm ignored");
    int sync = calendar.firstOn(today + 2) + 1;
    check(strcmp(calendar.events[sync].title, "Sync, weekly") == 0, "escaped comma");
    int holiday = calendar.firstOn(today + 9);
    check(holiday >= 0 && calendar.events[holiday].allDay, "all-day event");
    check(CalendarDate::isoWeek(today) == 41, "5 Oct 2026 is in week 41");
    check(CalendarDate::weekdayOf(today) == 0, "5 Oct 2026 is a Monday");
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
    checksFailed = 0;
    loadDemoCalendar(data);

    CalendarPage page;
    drawCalendar(canvas, data, page, "out/calendar-1-month.pbm");

    page.next();  // Tomorrow has no events
    drawCalendar(canvas, data, page, "out/calendar-2-free-day.pbm");

    page.showToday();
    page.toggleView();
    drawCalendar(canvas, data, page, "out/calendar-3-week.pbm");

    // 30 Nov 2026: November needs six rows of weeks
    page.toggleView();
    for (int i = 0; i < CalendarPage::MAX_DAYS_AHEAD; i++) {
        page.next();
    }
    drawCalendar(canvas, data, page, "out/calendar-4-six-weeks.pbm");

    // Before the first download
    data.calendar = CalendarData();
    page.showToday();
    drawCalendar(canvas, data, page, "out/calendar-5-not-loaded.pbm");

    if (checksFailed == 0) {
        printf("Calendar checks passed\n");
    }
}

int main() {
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

    printf("Saved preview images in out/\n");
    return 0;
}
