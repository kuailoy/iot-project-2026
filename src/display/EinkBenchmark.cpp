#include "EinkBenchmark.h"

namespace {

// How long each result stays on screen so you can judge it
constexpr unsigned long VIEW_MS = 4000;

enum class Pattern { WHITE, BLACK, CHECKER, TEXT };

struct Step {
    EinkMode mode;
    EinkLutId lut;     // Mode 2 only
    uint16_t hz;
    int16_t temp;      // EINK_TEMP_AUTO = sensor
    Pattern pattern;
    bool region;       // Partial window update of the counter area only
    uint32_t keepMs;   // Booster idle timeout
    const char* look;  // What to check on the panel
};

// Counter area used by the region test (x and w are multiples of 8)
constexpr int16_t COUNTER_X = 40;
constexpr int16_t COUNTER_Y = 320;
constexpr int16_t COUNTER_W = 400;
constexpr int16_t COUNTER_H = 64;

constexpr int16_t AUTO = EINK_TEMP_AUTO;
constexpr EinkMode KWR = EinkMode::KWR_STOCK;
constexpr EinkMode OTP = EinkMode::KW_OTP;
constexpr EinkMode LUT = EinkMode::KW_LUT;
constexpr EinkLutId PARTIAL = EinkLutId::PARTIAL;
constexpr EinkLutId FAST = EinkLutId::FAST_FULL;
constexpr EinkLutId CLEAN = EinkLutId::FULL_CLEAN;

const Step STEPS[] = {
    // Step 1: baseline
    {KWR, FAST, 50, AUTO, Pattern::TEXT, false, 0, "stock refresh, text sharp, no red"},

    // Step 2: black/white with the panel's own waveform
    {OTP, FAST, 50, AUTO, Pattern::WHITE, false, 0, "all white, no colour cast"},
    {OTP, FAST, 50, AUTO, Pattern::BLACK, false, 0, "all black, even, no red"},
    {OTP, FAST, 50, AUTO, Pattern::CHECKER, false, 0, "clean 16 px checkerboard"},
    {OTP, FAST, 50, AUTO, Pattern::TEXT, false, 0, "sharp text, white background"},

    // Step 3: our waveforms
    {LUT, CLEAN, 50, AUTO, Pattern::CHECKER, false, 0, "checkerboard, no ghost of the text"},
    {LUT, FAST, 50, AUTO, Pattern::TEXT, false, 0, "text, no checkerboard ghost"},
    {LUT, PARTIAL, 50, AUTO, Pattern::TEXT, false, 0, "only the counter changed, no flash"},
    {LUT, PARTIAL, 50, AUTO, Pattern::TEXT, true, 0, "region: counter changed, rest untouched"},

    // Step 4: booster kept on between quick updates (PON skipped after the first)
    {LUT, PARTIAL, 50, AUTO, Pattern::TEXT, false, 5000, "counter, booster kept on"},
    {LUT, PARTIAL, 50, AUTO, Pattern::TEXT, false, 5000, "counter, booster kept on"},
    {LUT, PARTIAL, 50, AUTO, Pattern::TEXT, false, 5000, "counter, booster kept on"},

    // Step 4: frame rate
    {LUT, FAST, 70, AUTO, Pattern::CHECKER, false, 0, "70 Hz: contrast vs 50 Hz"},
    {LUT, PARTIAL, 70, AUTO, Pattern::CHECKER, false, 0, "70 Hz partial"},
    {LUT, FAST, 100, AUTO, Pattern::TEXT, false, 0, "100 Hz: contrast vs 50 Hz"},
    {LUT, PARTIAL, 100, AUTO, Pattern::TEXT, false, 0, "100 Hz partial"},
    {OTP, FAST, 70, AUTO, Pattern::CHECKER, false, 0, "OTP at 70 Hz"},
    {OTP, FAST, 100, AUTO, Pattern::TEXT, false, 0, "OTP at 100 Hz"},

    // Step 4: forced temperature with the OTP waveform
    {OTP, FAST, 50, 25, Pattern::CHECKER, false, 0, "OTP forced 25 C"},
    {OTP, FAST, 50, 40, Pattern::TEXT, false, 0, "OTP forced 40 C"},
    {OTP, FAST, 50, 55, Pattern::CHECKER, false, 0, "OTP forced 55 C"},
    {OTP, FAST, 50, 70, Pattern::TEXT, false, 0, "OTP forced 70 C"},
    {OTP, FAST, 50, 80, Pattern::CHECKER, false, 0, "OTP forced 80 C"},

    // Leave the panel clean and white
    {LUT, CLEAN, 50, AUTO, Pattern::WHITE, false, 0, "all white, clean"},
};

constexpr int STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);

const char* patternName(Pattern p) {
    switch (p) {
        case Pattern::WHITE:
            return "white";
        case Pattern::BLACK:
            return "black";
        case Pattern::CHECKER:
            return "checker";
        default:
            return "text";
    }
}

void drawCounter(EinkDisplay& d, int number) {
    d.fillRect(COUNTER_X, COUNTER_Y, COUNTER_W, COUNTER_H, EPD_WHITE);
    d.setFont(nullptr);
    d.setTextSize(6);
    d.setTextColor(EPD_BLACK);
    d.setCursor(COUNTER_X + 8, COUNTER_Y + 8);
    d.printf("#%02d", number);
}

void drawPattern(EinkDisplay& d, Pattern pattern, int number, const Step& step) {
    d.clearBuffer();

    switch (pattern) {
        case Pattern::WHITE:
            d.fillScreen(EPD_WHITE);
            break;
        case Pattern::BLACK:
            d.fillScreen(EPD_BLACK);
            break;
        case Pattern::CHECKER:
            for (int16_t y = 0; y < d.height(); y += 16) {
                for (int16_t x = 0; x < d.width(); x += 16) {
                    bool black = ((x / 16) + (y / 16)) % 2 == 0;
                    d.fillRect(x, y, 16, 16, black ? EPD_BLACK : EPD_WHITE);
                }
            }
            break;
        case Pattern::TEXT: {
            d.fillScreen(EPD_WHITE);
            d.setFont(nullptr);
            d.setTextColor(EPD_BLACK);
            d.setTextSize(4);
            d.setCursor(40, 40);
            d.print("E-Ink benchmark");
            d.setTextSize(3);
            d.setCursor(40, 110);
            d.printf("%s  %u Hz", EinkDisplay::modeName(step.mode), step.hz);
            d.setCursor(40, 150);
            if (step.mode == EinkMode::KW_LUT) {
                d.print(EinkDisplay::lutSet(step.lut).name);
            } else {
                d.print("OTP waveform");
            }
            d.setTextSize(2);
            d.setCursor(40, 200);
            d.print("The quick brown fox jumps over the lazy dog 0123456789");
            d.drawRect(30, 30, 740, 420, EPD_BLACK);
            drawCounter(d, number);
            break;
        }
    }
}

struct Row {
    const Step* step;
    EinkTiming timing;
};

Row rows[STEP_COUNT];

void printTable(int count) {
    Serial.println();
    Serial.println("=== E-Ink benchmark (times in ms) ===");
    Serial.println(" #  mode       LUT         Hz  temp  pattern  region  keep   init  data   PON    DRF   POF  total  ok");
    for (int i = 0; i < count; i++) {
        const Step& s = *rows[i].step;
        const EinkTiming& t = rows[i].timing;
        char temp[6];
        if (s.temp == AUTO) {
            strcpy(temp, "auto");
        } else {
            snprintf(temp, sizeof(temp), "%d", s.temp);
        }
        const char* lut = s.mode == EinkMode::KW_LUT ? EinkDisplay::lutSet(s.lut).name : "OTP";
        Serial.printf("%2d  %-9s  %-10s %3u  %4s  %-7s  %-6s  %4lu  %5lu %5lu %5lu  %5lu %5lu  %5lu  %s\n",
                      i + 1, EinkDisplay::modeName(s.mode), lut, s.hz, temp, patternName(s.pattern),
                      s.region ? "yes" : "no", (unsigned long)s.keepMs, (unsigned long)t.init,
                      (unsigned long)t.data, (unsigned long)t.pon, (unsigned long)t.drf,
                      (unsigned long)t.pof, (unsigned long)t.total, t.ok ? "yes" : "NO");
    }
    Serial.println();
    Serial.println("Notes:");
    Serial.println("- DRF under ~200 ms in KW_OTP rows means the panel's OTP has no black/white");
    Serial.println("  waveform (nothing was driven). A timeout means the OTP data is unusable.");
    Serial.println("- PON = 0 means the booster was still on from the previous update.");
}

}  // namespace

void runEinkBenchmark(EinkDisplay& display) {
    Serial.println();
    Serial.println("=== E-Ink benchmark starting ===");
    EinkDisplay::printLutBalance();

    for (int i = 0; i < STEP_COUNT; i++) {
        const Step& step = STEPS[i];

        EinkConfig config;
        config.mode = step.mode;
        config.frameRateHz = step.hz;
        config.forceTempC = step.temp;
        config.keepPowerMs = step.keepMs;

        const EinkConfig& now = display.config();
        bool changed = now.mode != config.mode || now.frameRateHz != config.frameRateHz ||
                       now.forceTempC != config.forceTempC || now.keepPowerMs != config.keepPowerMs;
        if (changed && !display.setConfig(config)) {
            rows[i] = {&step, EinkTiming()};
            continue;
        }

        Serial.printf("\n[%d/%d] %s  -> look for: %s\n", i + 1, STEP_COUNT, patternName(step.pattern), step.look);

        if (step.region) {
            // Only the counter changes; the rest of the buffer stays as it was
            drawCounter(display, i + 1);
            display.updateRegion(COUNTER_X, COUNTER_Y, COUNTER_W, COUNTER_H);
        } else {
            drawPattern(display, step.pattern, i + 1, step);
            if (step.mode == EinkMode::KW_LUT) {
                display.fullRefresh(step.lut);
            } else {
                display.refresh();
            }
        }
        rows[i] = {&step, display.lastTiming()};

        // Keep-power rows run back to back; everything else waits so you can look
        bool nextKeepsPower = i + 1 < STEP_COUNT && STEPS[i + 1].keepMs > 0 && step.keepMs > 0;
        if (!nextKeepsPower) {
            delay(VIEW_MS);
        }
        display.tick();
    }

    display.sleep();
    printTable(STEP_COUNT);
    Serial.println("=== E-Ink benchmark done, panel is in deep sleep ===");
}
