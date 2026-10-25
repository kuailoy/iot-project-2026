// Renders UI pages to PBM images on a PC, no hardware needed.
// Run build.sh, output goes to tools/ui-preview/out/

#include <cstdio>
#include <cstring>

#include <Adafruit_GFX.h>

#include "Components.h"
#include "pages/HomePage.h"
#include "pages/MenuPage.h"

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

int main() {
    GFXcanvas1 canvas(SCREEN_W, SCREEN_H);
    AppData data = demoData();

    HomePage home;
    canvas.fillScreen(EPD_WHITE);
    home.draw(canvas, data);
    savePbm(canvas, "out/home.pbm");

    // Before Wi-Fi, NTP and sensors have delivered anything
    AppData empty;
    canvas.fillScreen(EPD_WHITE);
    home.draw(canvas, empty);
    savePbm(canvas, "out/home-empty.pbm");

    MenuPage menu;
    menu.move(1);
    canvas.fillScreen(EPD_WHITE);
    menu.draw(canvas, data);
    savePbm(canvas, "out/menu.pbm");

    printf("Saved out/home.pbm, out/home-empty.pbm and out/menu.pbm\n");
    return 0;
}
