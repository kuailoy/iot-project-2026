// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

#include <cstdio>
#include <cstring>

#include <Adafruit_GFX.h>

#include "Components.h"
#include "../app/DateUtil.h"
#include "pages/CalendarPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/SleepPage.h"

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

    // Calendar: today (busy), then one turn forward (Tuesday, free)
    AppData calendarData = data;
    calendarData.now.tm_min = 35;
    CalendarPage calendar;
    calendar.open();
    canvas.fillScreen(COLOR_BG);
    calendar.draw(canvas, calendarData);
    savePbm(canvas, "out/calendar-today.pbm");

    calendar.move(calendarData, 1);
    canvas.fillScreen(COLOR_BG);
    calendar.draw(canvas, calendarData);
    savePbm(canvas, "out/calendar-free.pbm");

    printf("Saved home, home-empty, menu, sleep, calendar-today and calendar-free (.pbm) in out/\n");
    return 0;
}
