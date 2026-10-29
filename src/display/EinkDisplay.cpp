#include "EinkDisplay.h"

#include <SPI.h>

// ---------- UC8179 commands (datasheet p14-16) ----------

namespace {

constexpr uint8_t CMD_PSR = 0x00;    // Panel setting
constexpr uint8_t CMD_PWR = 0x01;    // Power setting
constexpr uint8_t CMD_POF = 0x02;    // Power off
constexpr uint8_t CMD_PON = 0x04;    // Power on
constexpr uint8_t CMD_DSLP = 0x07;   // Deep sleep
constexpr uint8_t CMD_DTM1 = 0x10;   // KWR: black/white data, KW: OLD data
constexpr uint8_t CMD_DRF = 0x12;    // Display refresh
constexpr uint8_t CMD_DTM2 = 0x13;   // KWR: red data, KW: NEW data
constexpr uint8_t CMD_DUSPI = 0x15;  // Dual SPI
constexpr uint8_t CMD_LUTC = 0x20;   // VCOM LUT
constexpr uint8_t CMD_LUTWW = 0x21;
constexpr uint8_t CMD_LUTKW = 0x22;
constexpr uint8_t CMD_LUTWK = 0x23;
constexpr uint8_t CMD_LUTKK = 0x24;
constexpr uint8_t CMD_LUTBD = 0x25;  // Border LUT
constexpr uint8_t CMD_PLL = 0x30;    // Frame rate
constexpr uint8_t CMD_CDI = 0x50;    // VCOM and data interval
constexpr uint8_t CMD_TCON = 0x60;   // Gate/source non-overlap
constexpr uint8_t CMD_TRES = 0x61;   // Resolution
constexpr uint8_t CMD_VDCS = 0x82;   // VCOM_DC
constexpr uint8_t CMD_PTL = 0x90;    // Partial window
constexpr uint8_t CMD_PTIN = 0x91;   // Partial in
constexpr uint8_t CMD_PTOUT = 0x92;  // Partial out
constexpr uint8_t CMD_CCSET = 0xE0;  // Cascade setting (TSFIX)
constexpr uint8_t CMD_TSSET = 0xE5;  // Forced temperature

// PSR (p17). Low bits copied from the Adafruit driver (0x07): UD=0 (scan
// down), SHL=1, booster on, no soft reset. The Adafruit frame buffer is
// stored bottom row first to match UD=0, so UD must stay 0.
constexpr uint8_t PSR_BASE = 0x07;
constexpr uint8_t PSR_KW = 0x10;   // Black/white mode
constexpr uint8_t PSR_REG = 0x20;  // LUTs from registers instead of OTP

// CDI byte 1 (p27-28): border Hi-Z (as the Adafruit driver), DDX=00.
// With DDX=00 a 1 bit means black in both modes, which is how the Adafruit
// buffer stores pixels. In KW mode N2OCP copies NEW to OLD after a refresh.
constexpr uint8_t CDI_KWR = 0x90;
constexpr uint8_t CDI_KW = 0x98;   // CDI_KWR + N2OCP
constexpr uint8_t CDI_INTERVAL = 0x07;  // Default, 10 hsync

// Longest a single BUSY phase may take. Stock refresh is ~29 s at 25 C.
constexpr uint32_t BUSY_TIMEOUT_MS = 60000;

// Gate (vertical) line count is 480, rounded up in the controller
const SPISettings spiSettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0);

}  // namespace

// ---------- Setup ----------

bool EinkDisplay::begin(const EinkConfig& config) {
    // Adafruit setup: allocates the frame buffers, starts SPI, resets the
    // panel and leaves it in deep sleep
    ThinkInk_750_Tricolor_AABMFGNR::begin(THINKINK_TRICOLOR);

    pinMode(_cs_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_reset_pin, OUTPUT);
    pinMode(_busy_pin, INPUT);
    digitalWrite(_cs_pin, HIGH);

    if (oldFrame == nullptr) {
        oldFrame = (uint8_t*)malloc(FRAME_BYTES);
    }
    if (oldFrame == nullptr) {
        Serial.println("E-Ink: not enough RAM for the OLD frame buffer");
        return false;
    }
    memset(oldFrame, 0, FRAME_BYTES);

    powered = false;
    panelKnown = false;
    return setConfig(config);
}

bool EinkDisplay::setConfig(const EinkConfig& config) {
    if (!einkFrameRateOk(config.frameRateHz)) {
        Serial.printf("E-Ink: %u Hz is not allowed (PLL table / TCON limit)\n", config.frameRateHz);
        return false;
    }

    powerOff();
    // Switching between three-colour and black/white leaves the panel in an
    // unknown state for our partial waveform
    if (config.mode != cfg.mode) {
        panelKnown = false;
    }
    cfg = config;
    loadedLut = -1;
    return true;
}

const EinkLutSet& EinkDisplay::lutSet(EinkLutId id) {
    switch (id) {
        case EinkLutId::PARTIAL:
            return LUT_PARTIAL;
        case EinkLutId::FAST_FULL:
            return LUT_FAST_FULL;
        default:
            return LUT_FULL_CLEAN;
    }
}

const char* EinkDisplay::modeName(EinkMode mode) {
    switch (mode) {
        case EinkMode::KWR_STOCK:
            return "KWR_STOCK";
        case EinkMode::KW_OTP:
            return "KW_OTP";
        default:
            return "KW_LUT";
    }
}

void EinkDisplay::printLutBalance() {
    Serial.println("LUT DC imbalance (0.1 V x frames, must be 0):");
    for (EinkLutId id : {EinkLutId::PARTIAL, EinkLutId::FAST_FULL, EinkLutId::FULL_CLEAN}) {
        const EinkLutSet& s = lutSet(id);
        long ww = einkLutCharge(s.ww, s.vcom);
        long kw = einkLutCharge(s.kw, s.vcom);
        long wk = einkLutCharge(s.wk, s.vcom);
        long kk = einkLutCharge(s.kk, s.vcom);
        bool ok = ww == 0 && kw == 0 && wk == 0 && kk == 0;
        Serial.printf("  %-10s WW %ld  KW %ld  WK %ld  KK %ld  %s\n", s.name, ww, kw, wk, kk,
                      ok ? "OK" : "WARNING: NOT BALANCED");
    }
}

// ---------- Public refresh API ----------

bool EinkDisplay::refresh() {
    if (cfg.mode != EinkMode::KW_LUT) {
        return fullRefresh(EinkLutId::FAST_FULL);
    }
    if (!panelKnown || updatesSinceClean >= EPD_CLEAN_EVERY) {
        return fullRefresh(EinkLutId::FULL_CLEAN);
    }
    return fullRefresh(EinkLutId::PARTIAL);
}

bool EinkDisplay::fullRefresh(EinkLutId lut) {
    // PARTIAL compares OLD and NEW; if OLD is unknown it could leave wrong pixels
    if (cfg.mode == EinkMode::KW_LUT && lut == EinkLutId::PARTIAL && !panelKnown) {
        lut = EinkLutId::FULL_CLEAN;
    }
    return runRefresh(lut, nullptr);
}

bool EinkDisplay::updateRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (cfg.mode != EinkMode::KW_LUT || !panelKnown) {
        return refresh();
    }

    // Clip to the screen, widen x to whole bytes
    int16_t x0 = max<int16_t>(x, 0);
    int16_t y0 = max<int16_t>(y, 0);
    int16_t x1 = min<int16_t>(x + w - 1, W - 1);
    int16_t y1 = min<int16_t>(y + h - 1, H - 1);
    if (x1 < x0 || y1 < y0) {
        return true;  // Nothing visible to update
    }

    Window window;
    window.x0 = x0 & ~7;
    window.x1 = x1 | 7;
    window.y0 = y0;
    window.y1 = y1;
    return runRefresh(EinkLutId::PARTIAL, &window);
}

bool EinkDisplay::clean() {
    return runRefresh(EinkLutId::FULL_CLEAN, nullptr);
}

void EinkDisplay::sleep() {
    powerOff();
}

void EinkDisplay::tick() {
    if (powered && millis() - lastActive >= cfg.keepPowerMs) {
        powerOff();
    }
}

// ---------- Refresh ----------

bool EinkDisplay::runRefresh(EinkLutId lut, const Window* window) {
    timing = EinkTiming();
    unsigned long start = millis();
    bool kw = cfg.mode != EinkMode::KWR_STOCK;

    // 1. Wake up and set registers (datasheet p39: reset, then initialize)
    unsigned long stepStart = millis();
    if (!powered) {
        hardReset();
        if (!waitIdle("reset", nullptr)) {
            abortRefresh("reset");
            return false;
        }
        configure(lut);
    } else if (cfg.mode == EinkMode::KW_LUT && loadedLut != int8_t(lut)) {
        uploadLuts(lut);  // Booster still on, only the waveform changes
    }
    timing.init = millis() - stepStart;

    // 2. Image data
    stepStart = millis();
    if (window != nullptr) {
        uint8_t ptl[9] = {
            uint8_t(window->x0 >> 8), uint8_t(window->x0 & 0xF8),
            uint8_t(window->x1 >> 8), uint8_t(window->x1 | 0x07),
            uint8_t(window->y0 >> 8), uint8_t(window->y0 & 0xFF),
            uint8_t(window->y1 >> 8), uint8_t(window->y1 & 0xFF),
            0x01  // PT_SCAN: scan all gates (default)
        };
        command(CMD_PTIN);
        command(CMD_PTL, ptl, sizeof(ptl));
    }

    if (!kw) {
        sendFrame(CMD_DTM1, black_buffer, nullptr);
        sendFrame(CMD_DTM2, color_buffer, nullptr);
    } else {
        if (!controllerHasOld || window != nullptr) {
            sendFrame(CMD_DTM1, oldFrame, window);
        }
        sendFrame(CMD_DTM2, black_buffer, window);
    }
    timing.data = millis() - stepStart;

    // 3. Power on (p39: after data, before refresh)
    if (!powered) {
        command(CMD_PON);
        if (!waitIdle("PON", &timing.pon)) {
            abortRefresh("PON");
            return false;
        }
        powered = true;
    }

    // 4. Refresh
    command(CMD_DRF);
    if (!waitIdle("DRF", &timing.drf)) {
        abortRefresh("DRF");
        return false;
    }
    if (window != nullptr) {
        command(CMD_PTOUT);
    }

    // 5. Remember what the panel shows now
    if (kw) {
        if (window == nullptr) {
            memcpy(oldFrame, black_buffer, FRAME_BYTES);
        } else {
            for (uint16_t y = window->y0; y <= window->y1; y++) {
                uint32_t offset = uint32_t(H - 1 - y) * ROW_BYTES + window->x0 / 8;
                memcpy(oldFrame + offset, black_buffer + offset, (window->x1 - window->x0 + 1) / 8);
            }
        }
        // N2OCP copied NEW to OLD in the controller, but only trust it
        // after a whole-screen refresh
        controllerHasOld = (window == nullptr);
        panelKnown = true;
    }

    if (cfg.mode == EinkMode::KW_LUT && lut == EinkLutId::FULL_CLEAN) {
        updatesSinceClean = 0;
    } else {
        updatesSinceClean++;
    }

    // 6. Power off, or keep the booster on for a quick next update
    lastActive = millis();
    if (cfg.keepPowerMs == 0) {
        unsigned long pofStart = millis();
        powerOff();
        timing.pof = millis() - pofStart;
    }

    timing.total = millis() - start;
    timing.ok = true;
    logTiming(lut, window);
    return true;
}

void EinkDisplay::configure(EinkLutId lut) {
    bool kw = cfg.mode != EinkMode::KWR_STOCK;
    bool regLut = cfg.mode == EinkMode::KW_LUT;

    // Power (p17-18): VGH/VGL = +-20 V, VDH/VDL = +-15 V. These are the
    // Adafruit values and the datasheet maximum; not changed.
    command(CMD_PWR, {0x07, 0x07, 0x3F, 0x3F});

    uint8_t psr = PSR_BASE | (kw ? PSR_KW : 0) | (regLut ? PSR_REG : 0);
    command(CMD_PSR, {psr});

    command(CMD_PLL, {uint8_t(einkFrsCode(cfg.frameRateHz))});
    command(CMD_TRES, {0x03, 0x20, 0x01, 0xE0});  // 800 x 480
    command(CMD_DUSPI, {0x00});
    command(CMD_CDI, {kw ? CDI_KW : CDI_KWR, CDI_INTERVAL});
    uint8_t tcon = einkTconStep(cfg.frameRateHz);
    command(CMD_TCON, {uint8_t(tcon << 4 | tcon)});

    // Forced temperature only changes which OTP waveform is picked (p34-35)
    if (!regLut && cfg.forceTempC != EINK_TEMP_AUTO) {
        command(CMD_CCSET, {0x02});  // TSFIX: use TS_SET instead of the sensor
        command(CMD_TSSET, {uint8_t(int8_t(cfg.forceTempC))});
    }

    if (regLut) {
        command(CMD_VDCS, {EPD_VCOM});
        uploadLuts(lut);
    } else {
        loadedLut = -1;
    }
    controllerHasOld = false;  // Controller RAM was cleared by the reset
}

void EinkDisplay::uploadLuts(EinkLutId id) {
    const EinkLutSet& set = lutSet(id);

    // VCOM, KW, WK and KK take 10 groups (60 bytes), WW and border 7 groups
    // (42 bytes), p14-15. KW mode only runs the first 7, the rest stay 0.
    struct Entry {
        uint8_t cmd;
        const EinkLut* lut;
        uint8_t groups;
    };
    const Entry entries[] = {
        {CMD_LUTC, &set.vcom, 10}, {CMD_LUTWW, &set.ww, 7},  {CMD_LUTKW, &set.kw, 10},
        {CMD_LUTWK, &set.wk, 10},  {CMD_LUTKK, &set.kk, 10}, {CMD_LUTBD, &set.border, 7},
    };

    for (const Entry& e : entries) {
        uint8_t bytes[60] = {};
        for (int g = 0; g < EINK_LUT_GROUPS; g++) {
            const EinkLutGroup& group = e.lut->group[g];
            uint8_t* out = bytes + g * 6;
            out[0] = (group.level[0] & 3) << 6 | (group.level[1] & 3) << 4 |
                     (group.level[2] & 3) << 2 | (group.level[3] & 3);
            for (int p = 0; p < 4; p++) {
                out[1 + p] = group.frames[p];
            }
            out[5] = group.repeat;
        }
        command(e.cmd, bytes, e.groups * 6);
    }
    loadedLut = int8_t(id);
}

// Send a whole frame, or only the rows/bytes inside the window.
// Buffer rows are stored bottom row first (Adafruit, UD=0), so the window's
// rows are sent from y1 down to y0, which is the order the gates are scanned.
void EinkDisplay::sendFrame(uint8_t cmd, const uint8_t* frame, const Window* window) {
    SPI.beginTransaction(spiSettings);
    digitalWrite(_dc_pin, LOW);
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(cmd);
    digitalWrite(_dc_pin, HIGH);

    if (window == nullptr) {
        SPI.writeBytes(frame, FRAME_BYTES);
    } else {
        uint16_t bytes = (window->x1 - window->x0 + 1) / 8;
        for (int32_t y = window->y1; y >= window->y0; y--) {
            SPI.writeBytes(frame + uint32_t(H - 1 - y) * ROW_BYTES + window->x0 / 8, bytes);
        }
    }

    digitalWrite(_cs_pin, HIGH);
    SPI.endTransaction();
}

// ---------- Power ----------

void EinkDisplay::hardReset() {
    // p39: RST low > 50 us, then high and wait > 1 ms
    digitalWrite(_reset_pin, LOW);
    delay(1);
    digitalWrite(_reset_pin, HIGH);
    delay(2);
    powered = false;
    loadedLut = -1;
    controllerHasOld = false;
}

void EinkDisplay::powerOff() {
    if (!powered) {
        return;
    }
    command(CMD_POF);
    if (!waitIdle("POF", nullptr)) {
        abortRefresh("POF");
        return;
    }
    command(CMD_DSLP, {0xA5});  // Deep sleep, check code A5 (p20)
    powered = false;
    loadedLut = -1;
    controllerHasOld = false;   // Controller RAM is lost in deep sleep
}

// A step took far too long (e.g. an empty OTP waveform). Reset stops the
// controller and releases all outputs (p17, RST_N), so no voltage stays on.
void EinkDisplay::abortRefresh(const char* step) {
    Serial.printf("E-Ink: %s timed out after %lu ms, resetting the controller\n", step,
                  (unsigned long)BUSY_TIMEOUT_MS);
    hardReset();
    command(CMD_DSLP, {0xA5});
    panelKnown = false;
    timing.ok = false;
}

// BUSY_N is low while the controller works (p20-21). The time is measured
// from the command that started the step.
bool EinkDisplay::waitIdle(const char* step, uint32_t* elapsedMs) {
    (void)step;
    unsigned long start = millis();

    // BUSY drops shortly after the command; don't mistake the gap for "done"
    while (digitalRead(_busy_pin) == HIGH && millis() - start < 5) {
    }
    while (digitalRead(_busy_pin) == LOW) {
        if (millis() - start > BUSY_TIMEOUT_MS) {
            return false;
        }
        delay(1);
    }

    if (elapsedMs != nullptr) {
        *elapsedMs = millis() - start;
    }
    return true;
}

// ---------- SPI ----------

void EinkDisplay::command(uint8_t cmd) {
    command(cmd, nullptr, 0);
}

void EinkDisplay::command(uint8_t cmd, std::initializer_list<uint8_t> data) {
    command(cmd, data.begin(), data.size());
}

void EinkDisplay::command(uint8_t cmd, const uint8_t* data, uint32_t length) {
    // Never send a command while the controller is busy
    if (digitalRead(_busy_pin) == LOW) {
        unsigned long start = millis();
        while (digitalRead(_busy_pin) == LOW && millis() - start < BUSY_TIMEOUT_MS) {
            delay(1);
        }
    }

    SPI.beginTransaction(spiSettings);
    digitalWrite(_dc_pin, LOW);
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(cmd);
    if (length > 0) {
        digitalWrite(_dc_pin, HIGH);
        SPI.writeBytes(data, length);
    }
    digitalWrite(_cs_pin, HIGH);
    SPI.endTransaction();
}

// ---------- Logging ----------

void EinkDisplay::logTiming(EinkLutId lut, const Window* window) const {
    char temp[8];
    if (cfg.forceTempC == EINK_TEMP_AUTO) {
        strcpy(temp, "auto");
    } else {
        snprintf(temp, sizeof(temp), "%dC", cfg.forceTempC);
    }

    const char* lutName = cfg.mode == EinkMode::KW_LUT ? lutSet(lut).name : "OTP";
    Serial.printf("E-Ink %s %s%s %uHz temp=%s: init %lu  data %lu  PON %lu  DRF %lu  POF %lu  total %lu ms\n",
                  modeName(cfg.mode), lutName, window != nullptr ? " (region)" : "", cfg.frameRateHz,
                  temp, (unsigned long)timing.init, (unsigned long)timing.data, (unsigned long)timing.pon,
                  (unsigned long)timing.drf, (unsigned long)timing.pof, (unsigned long)timing.total);
}
