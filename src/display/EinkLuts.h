#pragma once

#include <stdint.h>

// Waveforms (LUTs) for the UC8179 in black/white (KW) mode.
// Datasheet C22259-001 v1.3: LUT format p22-23, KW transition table p28.
//
// A LUT has up to 7 groups (KW mode uses 7). Each group has 4 phases;
// every phase applies one voltage level for a number of frames, and the
// whole group runs "repeat" times. One frame lasts 1 / frame rate
// (20 ms at 50 Hz), so 50 frames = 1 s at 50 Hz.
//
// All six LUTs of a set run side by side, so they must have the same
// timing: only the levels may differ. This is checked at compile time.
//
// Tuning: change frames or repeats here, nothing else needs to change.
// Longer drive = better contrast and less ghosting, but slower.

// Level codes (2 bits). Pixel LUTs: GND / VDH / VDL / VDHR.
// VCOM LUT: VCOM_DC / VDH+VCOM_DC / VDL+VCOM_DC / floating.
enum EinkLevel : uint8_t {
    LV_0 = 0,   // GND (VCOM LUT: VCOM_DC), no drive
    LV_H = 1,   // VDH = +15 V, drives a pixel towards black
    LV_L = 2,   // VDL = -15 V, drives a pixel towards white
    LV_R = 3    // VDHR = +3 V, red pixel voltage, unused in KW mode
};

struct EinkLutGroup {
    uint8_t level[4];   // EinkLevel per phase
    uint8_t frames[4];  // Frames per phase, 0..255
    uint8_t repeat;     // How many times the group runs (0 = skipped)
};

constexpr int EINK_LUT_GROUPS = 7;

struct EinkLut {
    EinkLutGroup group[EINK_LUT_GROUPS];  // Unused groups stay all zero
};

// One complete waveform: what every OLD -> NEW pixel transition does.
// kw = black to white, wk = white to black, ww / kk = pixel stays the same.
struct EinkLutSet {
    const char* name;
    EinkLut vcom, ww, kw, wk, kk, border;
};

// Short way to write a group: level and frames for each phase, then repeat
constexpr EinkLutGroup lutGroup(uint8_t l0, uint8_t f0, uint8_t l1, uint8_t f1,
                                uint8_t l2, uint8_t f2, uint8_t l3, uint8_t f3,
                                uint8_t repeat) {
    return EinkLutGroup{{l0, l1, l2, l3}, {f0, f1, f2, f3}, repeat};
}

// ---------- LUT sets ----------
// Frame counts are for the default 50 Hz frame rate.

// PARTIAL: only pixels that change are driven, the rest get no voltage.
// Taken from GxEPD2 (GxEPD2_750_T7.cpp, lut_20..25_partial, T1=30 T2=5 T3=30 T4=5),
// which is tuned for the black/white GDEW075T7 panel. Lengthen the phases
// if changed pixels look grey on this tri-colour panel.
// 70 frames = 1.4 s at 50 Hz.
constexpr EinkLutSet LUT_PARTIAL = {
    "PARTIAL",
    /* vcom   */ {{lutGroup(LV_0, 30, LV_0, 5, LV_0, 30, LV_0, 5, 1)}},
    /* ww     */ {{lutGroup(LV_0, 30, LV_0, 5, LV_0, 30, LV_0, 5, 1)}},
    /* kw     */ {{lutGroup(LV_H, 30, LV_H, 5, LV_L, 30, LV_L, 5, 1)}},  // Pre-pulse black, then white
    /* wk     */ {{lutGroup(LV_L, 30, LV_0, 5, LV_H, 30, LV_0, 5, 1)}},  // Pre-pulse white, then black
    /* kk     */ {{lutGroup(LV_0, 30, LV_0, 5, LV_0, 30, LV_0, 5, 1)}},
    /* border */ {{lutGroup(LV_0, 30, LV_0, 5, LV_0, 30, LV_0, 5, 1)}},
};

// FAST_FULL: every pixel flashes once to the opposite colour and back,
// which clears most ghosting. 60 frames = 1.2 s at 50 Hz.
constexpr EinkLutSet LUT_FAST_FULL = {
    "FAST_FULL",
    /* vcom   */ {{lutGroup(LV_0, 30, LV_0, 30, LV_0, 0, LV_0, 0, 1)}},
    /* ww     */ {{lutGroup(LV_H, 30, LV_L, 30, LV_0, 0, LV_0, 0, 1)}},
    /* kw     */ {{lutGroup(LV_H, 30, LV_L, 30, LV_0, 0, LV_0, 0, 1)}},
    /* wk     */ {{lutGroup(LV_L, 30, LV_H, 30, LV_0, 0, LV_0, 0, 1)}},
    /* kk     */ {{lutGroup(LV_L, 30, LV_H, 30, LV_0, 0, LV_0, 0, 1)}},
    /* border */ {{lutGroup(LV_0, 30, LV_0, 30, LV_0, 0, LV_0, 0, 1)}},
};

// FULL_CLEAN: several short black/white flashes to shake loose ghosting
// and red-particle residue, then one long settle phase.
// 3 x 40 + 80 = 200 frames = 4 s at 50 Hz.
constexpr EinkLutSet LUT_FULL_CLEAN = {
    "FULL_CLEAN",
    /* vcom   */ {{lutGroup(LV_0, 10, LV_0, 10, LV_0, 10, LV_0, 10, 3),
                   lutGroup(LV_0, 40, LV_0, 40, LV_0, 0, LV_0, 0, 1)}},
    /* ww     */ {{lutGroup(LV_H, 10, LV_L, 10, LV_H, 10, LV_L, 10, 3),
                   lutGroup(LV_H, 40, LV_L, 40, LV_0, 0, LV_0, 0, 1)}},
    /* kw     */ {{lutGroup(LV_H, 10, LV_L, 10, LV_H, 10, LV_L, 10, 3),
                   lutGroup(LV_H, 40, LV_L, 40, LV_0, 0, LV_0, 0, 1)}},
    /* wk     */ {{lutGroup(LV_L, 10, LV_H, 10, LV_L, 10, LV_H, 10, 3),
                   lutGroup(LV_L, 40, LV_H, 40, LV_0, 0, LV_0, 0, 1)}},
    /* kk     */ {{lutGroup(LV_L, 10, LV_H, 10, LV_L, 10, LV_H, 10, 3),
                   lutGroup(LV_L, 40, LV_H, 40, LV_0, 0, LV_0, 0, 1)}},
    /* border */ {{lutGroup(LV_0, 10, LV_0, 10, LV_0, 10, LV_0, 10, 3),
                   lutGroup(LV_0, 40, LV_0, 40, LV_0, 0, LV_0, 0, 1)}},
};

// ---------- Compile-time checks ----------
// DC balance: for every transition, the sum of (pixel voltage x frames)
// must be 0, otherwise charge builds up in the panel and damages it over
// time. Pixel voltage = source level - VCOM level. Unit: 0.1 V x frame.

constexpr long einkLevelVolts(uint8_t level) {
    return level == LV_H ? 150 : level == LV_L ? -150 : level == LV_R ? 30 : 0;
}

// VCOM LUT: 00 = VCOM_DC (0 V relative), 11 = floating (counted as 0)
constexpr long einkVcomVolts(uint8_t level) {
    return level == LV_H ? 150 : level == LV_L ? -150 : 0;
}

constexpr long einkGroupCharge(const EinkLutGroup& pixel, const EinkLutGroup& vcom, int phase) {
    return phase == 4 ? 0
                      : (einkLevelVolts(pixel.level[phase]) - einkVcomVolts(vcom.level[phase])) *
                                pixel.frames[phase] +
                            einkGroupCharge(pixel, vcom, phase + 1);
}

constexpr long einkLutCharge(const EinkLut& pixel, const EinkLut& vcom, int group = 0) {
    return group == EINK_LUT_GROUPS
               ? 0
               : einkGroupCharge(pixel.group[group], vcom.group[group], 0) * pixel.group[group].repeat +
                     einkLutCharge(pixel, vcom, group + 1);
}

constexpr bool einkSameGroupTiming(const EinkLutGroup& a, const EinkLutGroup& b) {
    return a.repeat == b.repeat && a.frames[0] == b.frames[0] && a.frames[1] == b.frames[1] &&
           a.frames[2] == b.frames[2] && a.frames[3] == b.frames[3];
}

constexpr bool einkSameTiming(const EinkLut& a, const EinkLut& b, int group = 0) {
    return group == EINK_LUT_GROUPS ||
           (einkSameGroupTiming(a.group[group], b.group[group]) && einkSameTiming(a, b, group + 1));
}

constexpr bool einkSetTimingOk(const EinkLutSet& s) {
    return einkSameTiming(s.vcom, s.ww) && einkSameTiming(s.vcom, s.kw) &&
           einkSameTiming(s.vcom, s.wk) && einkSameTiming(s.vcom, s.kk) &&
           einkSameTiming(s.vcom, s.border);
}

constexpr bool einkSetBalanced(const EinkLutSet& s) {
    return einkLutCharge(s.ww, s.vcom) == 0 && einkLutCharge(s.kw, s.vcom) == 0 &&
           einkLutCharge(s.wk, s.vcom) == 0 && einkLutCharge(s.kk, s.vcom) == 0;
}

// Set -D EPD_ALLOW_UNBALANCED=1 only for debugging a waveform on purpose
#ifndef EPD_ALLOW_UNBALANCED
#define EPD_ALLOW_UNBALANCED 0
#endif

static_assert(einkSetTimingOk(LUT_PARTIAL), "LUT_PARTIAL: all six LUTs need the same frames/repeats");
static_assert(einkSetTimingOk(LUT_FAST_FULL), "LUT_FAST_FULL: all six LUTs need the same frames/repeats");
static_assert(einkSetTimingOk(LUT_FULL_CLEAN), "LUT_FULL_CLEAN: all six LUTs need the same frames/repeats");

static_assert(EPD_ALLOW_UNBALANCED || einkSetBalanced(LUT_PARTIAL), "LUT_PARTIAL is not DC-balanced");
static_assert(EPD_ALLOW_UNBALANCED || einkSetBalanced(LUT_FAST_FULL), "LUT_FAST_FULL is not DC-balanced");
static_assert(EPD_ALLOW_UNBALANCED || einkSetBalanced(LUT_FULL_CLEAN), "LUT_FULL_CLEAN is not DC-balanced");
