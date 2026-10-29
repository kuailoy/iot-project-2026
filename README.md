# IoT Group Project

This repository is used for managing and developing our group IoT project.

## Components

- [ESP32 + BMS — Seeed Studio XIAO ESP32S3 (113991114)](https://www.mouser.fi/en/ProductDetail/Seeed-Studio/113991114)
- [E-ink controller — Adafruit eInk Breakout Friend with 32KB SRAM (4224)](https://www.digikey.fi/en/products/detail/adafruit-industries-llc/4224/10107219)
- [E-ink display — Adafruit 7.5" 800×480 Tri-Color eInk / ePaper — Bare Display (6415)](https://www.digikey.fi/en/products/detail/adafruit-industries-llc/6415/27545882)
- [Battery — Jauch LP305166JH Lithium Polymer Rechargeable Battery](https://www.digikey.fi/en/products/detail/jauch-quartz/LP305166JH-PCM-2-WIRES-70MM/13540143)
- [Touch button — SunFounder TTP223 Touch Sensor Module (ST0246)](https://www.digikey.fi/en/products/detail/sunfounder/ST0246/22116807)
- [Sensors (CO2, Humidity, Temperature) — Adafruit SCD-41 True CO2 Temperature and Humidity Sensor — STEMMA QT / Qwiic (5190)](https://www.digikey.fi/en/products/detail/adafruit-industries-llc/5190/15194670)
- [Rotary encoder — Adafruit I2C STEMMA QT Rotary Encoder Breakout with Encoder (5880)](https://www.digikey.fi/fi/products/detail/adafruit-industries-llc/5880/22596384)

## Workflow

Keep the project management simple:

**If it is a task, create an Issue.**

**If it is code, create a Pull Request.**

**If it is a meeting, add a meeting note.**

Basic workflow:

```text
Issue → Work → Pull Request → Review → Merge
```

Use **Issues** for tasks, problems, ideas, and things that need discussion.

Use **Pull Requests (PRs)** for code changes and other changes that need review.

When a task is completed, close the related Issue.

## Repository Structure

```text
.
├── platformio.ini     # PlatformIO board, framework and library configuration
├── include/           # Wiring definitions and local secrets template
├── src/               # Firmware source (app, services and UI)
├── tools/             # Font generator and desktop UI preview
├── assets/            # UI reference images and project visuals
├── docs/              # Project documentation
└── meetings/          # Meeting notes and decisions
```

## Project Timeline

| Milestone               | Deadline |
| ----------------------- | -------- |
| Project proposal        | Sep 30      |
| Project plan review.    | OCT 8        |
| Preliminary technical report | OCT 31      |
| Final technical report    | DEC 8      |
| Final presentation (Trade Fair)     | DEC 10      |

##  Git Instructions

Check [git quick reference](/docs/git-quick-reference.md)

## E-Ink display driver

Panel: ZJY800480-075AABMFGN-R (7.5", 800×480, black/white/red, UC8179 controller).
Driver: `src/display/` — `EinkDisplay` (refresh), `EinkLuts.h` (waveforms), `EinkBenchmark` (timing test).
Register values follow the panel datasheet (C22259-001 v1.3); page numbers are in the code comments.

### Switching modes

Set the flags in `platformio.ini`, then flash as usual:

| Flag | Values |
| --- | --- |
| `EPD_MODE` | `0` stock black/white/red (~29 s) · `1` black/white with the panel's own waveform · `2` black/white with our fast waveforms (default) |
| `EPD_FRAME_RATE` | `50` (default), `70`, `100`, … Every waveform gets shorter at a higher rate, but contrast may drop |
| `EPD_FORCE_TEMP` | Modes 0/1: temperature in °C used to pick the panel's waveform, `-128` = off |
| `EPD_KEEP_POWER_MS` | Keep the booster on for quick follow-up updates, `0` = power off after every refresh |
| `EPD_CLEAN_EVERY` | Mode 2: deep-clean refresh after this many fast updates (default 10) |
| `EPD_BENCHMARK` | `1` = run the benchmark instead of the app and print a timing table on the serial monitor |

In mode 2 the UI uses the `PARTIAL` waveform (only changed pixels are driven), `FULL_CLEAN` at start-up and every `EPD_CLEAN_EVERY` updates.
Code can also call `fullRefresh(EinkLutId::FAST_FULL)`, `updateRegion(x, y, w, h)`, `clean()` and `sleep()`.

### Tuning the waveforms

All waveforms are in `src/display/EinkLuts.h` as data: a level and a frame count for each phase.
Frames are 20 ms at 50 Hz.

- Changed pixels look grey, or ghosts stay: make the phases longer.
- It's too slow: make them shorter, or raise `EPD_FRAME_RATE`.
- The build fails if a waveform is not DC-balanced (it would damage the panel over time) or if the six LUTs of one set have different timing.

The `PARTIAL` waveform comes from GxEPD2 (`GxEPD2_750_T7`), which is tuned for a black/white panel.
This panel is tri-colour, so it may need longer phases.

### Measured results

Not measured yet. Run with `EPD_BENCHMARK=1` and paste the table here.
Expected waveform time at 50 Hz: PARTIAL 1.4 s, FAST_FULL 1.2 s, FULL_CLEAN 4 s, plus ~0.2 s for power on/off and data.

### Known trade-offs

- Black/white modes can't show red.
- Fast waveforms leave some ghosting; `FULL_CLEAN` removes it, with a few seconds of flashing.
- If a red tint builds up over time, switch to `EPD_MODE=0` once: the stock waveform resets the red particles.
- VCOM is fixed at −2.0 V (`EPD_VCOM`). This panel's real value is unknown, and the breakout probably can't read it back.
- `updateRegion` saves only data transfer time; the waveform takes as long as a full-screen partial update.
- If region updates appear in the wrong place, the vertical window mapping in `EinkDisplay::runRefresh` is the first suspect.
- The panel is rated for 1,000,000 updates; a clock that refreshes every minute uses about 525,000 a year.
