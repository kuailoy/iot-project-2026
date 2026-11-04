#pragma once

#include <initializer_list>

#include <Adafruit_ThinkInk.h>

#include "EinkLuts.h"

// ---------- Build options (platformio.ini, -D NAME=value) ----------

// 0 = stock black/white/red, panel's own waveform (~29 s)
// 1 = black/white, panel's own (OTP) waveform
// 2 = black/white, our waveforms from EinkLuts.h (fast, partial updates)
#ifndef EPD_MODE
#define EPD_MODE 0
#endif

// Frame rate in Hz: 5 10 15 20 30 40 50 60 70 80 90 100 110 130 150.
// Waveform phases are counted in frames, so 100 Hz halves every phase.
#ifndef EPD_FRAME_RATE
#define EPD_FRAME_RATE 50
#endif

// Force the temperature (C) the panel uses to pick its OTP waveform,
// modes 0 and 1 only. -128 = off, use the built-in sensor.
#ifndef EPD_FORCE_TEMP
#define EPD_FORCE_TEMP -128
#endif

// Keep the booster on for this long after a refresh so quick updates skip
// power-on. 0 = power off and deep sleep right after every refresh.
#ifndef EPD_KEEP_POWER_MS
#define EPD_KEEP_POWER_MS 0
#endif

// Mode 2: after this many fast/partial updates, refresh() runs FULL_CLEAN
#ifndef EPD_CLEAN_EVERY
#define EPD_CLEAN_EVERY 10
#endif

// Mode 2: VCOM_DC register value (datasheet p32), 0x26 = -2.0 V
#ifndef EPD_VCOM
#define EPD_VCOM 0x26
#endif

// Mode 2: which waveform set from EinkLuts.h. 0 = normal, 1 = fast
// (shorter phases, ~40% quicker, fainter and more ghosting)
#ifndef EPD_WAVEFORM
#define EPD_WAVEFORM 0
#endif

// SPI clock. The datasheet allows at most 10 MHz (100 ns cycle, p12).
#ifndef EPD_SPI_HZ
#define EPD_SPI_HZ 10000000
#endif

// ---------- Types ----------

enum class EinkMode : uint8_t {
    KWR_STOCK = 0,  // Black/white/red, OTP waveform
    KW_OTP = 1,     // Black/white, OTP waveform
    KW_LUT = 2      // Black/white, waveforms from EinkLuts.h
};

enum class EinkLutId : uint8_t { PARTIAL, FAST_FULL, FULL_CLEAN };

constexpr int16_t EINK_TEMP_AUTO = -128;

struct EinkConfig {
    EinkMode mode = EinkMode(EPD_MODE);
    uint16_t frameRateHz = EPD_FRAME_RATE;
    int16_t forceTempC = EPD_FORCE_TEMP;
    uint32_t keepPowerMs = EPD_KEEP_POWER_MS;
};

// How long each step of the last refresh took, in ms
struct EinkTiming {
    uint32_t init = 0;   // Reset + registers + LUT upload
    uint32_t data = 0;   // Sending the image over SPI
    uint32_t pon = 0;    // BUSY low after power on (0 if the booster was still on)
    uint32_t drf = 0;    // BUSY low during the refresh itself
    uint32_t pof = 0;    // BUSY low after power off (0 if the booster stays on)
    uint32_t total = 0;
    bool ok = false;
};

// ---------- Frame rate (PLL p25, TCON p30) ----------
// A frame is 620 lines. Each line is split into source-to-gate gap,
// gate-on time and gate-to-source gap (TCON, step = 4 x 667 ns).
// The gaps are kept to at most a quarter line each, and at least 5 us
// of gate-on time must remain.

constexpr int einkFrsCode(uint32_t hz) {
    return hz == 5 ? 0 : hz == 10 ? 1 : hz == 15 ? 2 : hz == 20 ? 3 : hz == 30 ? 4
         : hz == 40 ? 5 : hz == 50 ? 6 : hz == 60 ? 7 : hz == 70 ? 8 : hz == 80 ? 9
         : hz == 90 ? 10 : hz == 100 ? 11 : hz == 110 ? 12 : hz == 130 ? 13
         : hz == 150 ? 14 : hz == 200 ? 15 : -1;
}

constexpr long einkLineNs(uint32_t hz) { return 1000000000L / (long(hz) * 620L); }
constexpr long einkTconRaw(uint32_t hz) { return einkLineNs(hz) / 4 / 2668 - 1; }
constexpr uint8_t einkTconStep(uint32_t hz) {
    return einkTconRaw(hz) < 0 ? 0 : einkTconRaw(hz) > 15 ? 15 : uint8_t(einkTconRaw(hz));
}
constexpr long einkGateOnNs(uint32_t hz) {
    return einkLineNs(hz) - 2L * (einkTconStep(hz) + 1) * 4 * 667;
}
constexpr bool einkFrameRateOk(uint32_t hz) {
    return einkFrsCode(hz) >= 0 && einkGateOnNs(hz) >= 5000;
}

static_assert(einkFrameRateOk(EPD_FRAME_RATE), "EPD_FRAME_RATE: not in the PLL table or too fast");
static_assert(EPD_SPI_HZ <= 10000000, "EPD_SPI_HZ: the controller allows at most 10 MHz");
static_assert(EPD_MODE >= 0 && EPD_MODE <= 2, "EPD_MODE must be 0, 1 or 2");
static_assert(EPD_WAVEFORM == 0 || EPD_WAVEFORM == 1, "EPD_WAVEFORM must be 0 or 1");

// ---------- Driver ----------

// 7.5" 800x480 ZJY800480-075AABMFGN-R panel, UC8179 controller.
// Datasheet: C22259-001 v1.3, page numbers in the comments.
//
// Drawing is unchanged: this is the Adafruit class, so all GFX calls and
// the frame buffer work as before. Only the refresh is replaced, with
// timing for every step and black/white modes.
class EinkDisplay : public ThinkInk_750_Tricolor_AABMFGNR {
public:
    EinkDisplay(int16_t dc, int16_t rst, int16_t cs, int16_t srcs, int16_t busy)
        : ThinkInk_750_Tricolor_AABMFGNR(dc, rst, cs, srcs, busy) {}

    // Call once. Returns false if the config is invalid or RAM is short.
    bool begin(const EinkConfig& config = EinkConfig());

    // Change mode / frame rate / temperature / power timeout.
    // Powers the panel down first.
    bool setConfig(const EinkConfig& config);
    const EinkConfig& config() const { return cfg; }

    // Update the screen from the buffer and wait until it is done. Picks the
    // waveform itself: mode 2 uses PARTIAL, and FULL_CLEAN at start and every
    // EPD_CLEAN_EVERY updates.
    bool refresh();

    // Same, but returns as soon as the panel has started (after ~0.1-0.3 s),
    // so the app keeps reading input. Call tick() from loop() to finish it.
    // interactive = someone is using the controls: no FULL_CLEAN now.
    bool startRefresh(bool interactive);

    // True while the panel is still updating; don't start another refresh yet.
    // Drawing into the buffer is fine, the image is already in the panel.
    bool isBusy() const { return refreshing; }

    // Whole-screen refresh. The LUT only matters in mode 2.
    bool fullRefresh(EinkLutId lut);

    // Update only this rectangle with the PARTIAL waveform (mode 2).
    // x and w are widened to whole bytes (8 pixels). Other modes do a full refresh.
    bool updateRegion(int16_t x, int16_t y, int16_t w, int16_t h);

    // Remove ghosting: FULL_CLEAN in mode 2, a normal full refresh otherwise
    bool clean();

    // Power off and deep sleep now
    void sleep();

    // Call from loop(): finishes a started refresh, and powers down once
    // EPD_KEEP_POWER_MS have passed
    void tick();

    const EinkTiming& lastTiming() const { return timing; }

    // The set picked by EPD_WAVEFORM
    static const EinkLutSet& lutSet(EinkLutId id);
    static const char* modeName(EinkMode mode);

    // Print the DC imbalance of every LUT in both sets (0 = balanced)
    static void printLutBalance();

private:
    struct Window {
        uint16_t x0, x1, y0, y1;  // Inclusive, x0 and x1 + 1 multiples of 8
    };

    static constexpr uint16_t W = 800;
    static constexpr uint16_t H = 480;
    static constexpr uint32_t FRAME_BYTES = uint32_t(W) * H / 8;
    static constexpr uint16_t ROW_BYTES = W / 8;

    EinkConfig cfg;
    EinkTiming timing;
    uint8_t* oldFrame = nullptr;     // What the panel shows now (KW "OLD" data)

    bool powered = false;            // Booster on, registers set
    int8_t loadedLut = -1;           // LUT set in the controller registers
    bool controllerHasOld = false;   // Controller RAM already holds OLD (N2OCP)
    bool panelKnown = false;         // oldFrame matches the panel
    uint16_t updatesSinceClean = 0;
    unsigned long lastActive = 0;

    // Refresh in progress (startRefresh)
    bool refreshing = false;
    unsigned long refreshStart = 0;
    unsigned long drfStart = 0;
    EinkLutId refreshLut = EinkLutId::PARTIAL;
    Window refreshWindow = {0, 0, 0, 0};
    bool refreshHasWindow = false;

    EinkLutId chooseLut(bool interactive) const;
    bool runRefresh(EinkLutId lut, const Window* window);
    bool beginRefresh(EinkLutId lut, const Window* window);
    bool pollRefresh();
    void configure(EinkLutId lut);
    void uploadLuts(EinkLutId lut);
    void sendFrame(uint8_t cmd, const uint8_t* frame, const Window* window);
    bool waitIdle(const char* step, uint32_t* elapsedMs);
    void hardReset();
    void powerOff();
    void abortRefresh(const char* step);
    void logTiming(EinkLutId lut, const Window* window) const;

    void command(uint8_t cmd);
    void command(uint8_t cmd, const uint8_t* data, uint32_t length);
    void command(uint8_t cmd, std::initializer_list<uint8_t> data);
};
