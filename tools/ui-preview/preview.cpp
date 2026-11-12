// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <Adafruit_GFX.h>

#include "Components.h"
#include "../app/DateUtil.h"
#include "pages/CalendarPage.h"
#include "pages/ElectricityPage.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"
#include "pages/SettingsPage.h"
#include "pages/SetupPage.h"
#include "pages/WidgetPage.h"
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
    return 0;
}
