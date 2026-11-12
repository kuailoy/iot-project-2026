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

## Wi-Fi setup

On the first start (no home Wi-Fi saved) the screen shows a QR code and three steps:

1. Scan the QR code with the phone camera: the phone joins the device's own Wi-Fi, `Desk-Setup-XXXX`
   (its password is on the screen too, made once on the first start).
2. The phone opens the setup page by itself. If it doesn't, open `http://192.168.4.1`.
3. Choose the home Wi-Fi, type its password, then "Connect".
   The device tries the network; if it works it is saved and the device goes to the Home page,
   otherwise the page and the screen say what went wrong.

This page is only for the first setup (or when the saved Wi-Fi is gone: menu → Settings → press the knob).
Everything else is on the settings page below.

## Settings page (desk.local)

With the phone on the same Wi-Fi, open `http://desk.local` (or the IP / QR code from menu → Settings):

- **Widgets** of the second home page (see below)
- **Weather city**: search by name (OpenWeatherMap geocoding, uses `WEATHER_API_KEY`) and pick one of the matches
- **Calendar link**
- **Wi-Fi**: switch to another network. If the new one doesn't work within ~20 s, the display goes back to the old one.

There is no login: anyone on the same Wi-Fi can open the page. Other web sites can't use it through your browser
(the forms need a token from the page itself), and the saved calendar link is never shown again.
All settings are kept in the ESP32's flash (`src/app/Settings.cpp`), not in the code.
Only 2.4 GHz networks work; WPA2-Enterprise networks such as eduroam are not supported.
Code: `src/services/SetupPortal.cpp` (setup Wi-Fi, web page), `src/ui/pages/SetupPage.cpp` (screen).

## Second home page (widgets)

The touch button switches between the normal Home page and a second page made of widgets.

**Turning off**: on either home page, hold the touch button for 3 seconds. The screen shows "Turned off" with the time,
and once you lift your finger the device goes into deep sleep (Wi-Fi, CO2 sensor and screen off; E-Ink keeps the page without power).
Touch the button again to turn it on: it starts up like after a reset, in 10 to 20 seconds.
Code: `powerOff()` and the touch handling in `handleInput()` in `src/main.cpp`, `src/ui/pages/PowerOffPage.cpp`.
A tap now acts when the finger is lifted (so a hold isn't also a tap).
Its layout is edited on the settings page (`http://desk.local` → Widgets).
The editor works like a phone home screen: drag widgets to reorder them, tap one to change its size (Small 1×1, Medium 2×1, Large 2×2, Wide 4×1), move it or remove it, and add new ones.
Widgets: clock, date, weather, indoor (temperature, humidity, CO2), CO2, next event, today's events, Claude usage, electricity price.
The large and wide weather widgets also show feels like, wind (m/s and where it comes from), sunrise and sunset,
all from the same OpenWeatherMap request.

**Claude widget**: how much of your Claude plan's 5-hour session and weekly limit is used, with reset times.
Claude has no official API for this, so a small Chrome extension reads it in your logged-in browser and sends
only the numbers to the display (`POST /claude`). Setup: `tools/desk-extension/README.md`.

**Electricity widget**: the spot price now and whether it is cheap; bigger sizes add the day's chart and the cheapest 3 hours.

**Now playing widget** and the **Music** page (menu → Music): what YouTube Music plays in your browser, with a progress bar,
sent by the same Chrome extension (`POST /music`). Setup: `tools/desk-extension/README.md`.
Code: `src/services/NowPlaying.*` (message), `src/app/MusicData.h`, `src/ui/pages/MusicPage.cpp`, `drawMusic()` in `src/ui/pages/WidgetPage.cpp`.

**GitHub widget**: your contribution graph of the last year, like on your GitHub profile (a column per week, Sunday at the top).
The screen has no greens, so busier days are bigger black squares and days without contributions are small dots.
Next to it: contributions today, in the last year, and the current streak (the large size shows the last two as big numbers).
It needs a GitHub personal access token: github.com → Settings → Developer settings → Personal access tokens.
A fine-grained token without any permissions is enough (public contributions); paste it on the settings page (`desk.local`),
or set `GITHUB_TOKEN` in `.env.local`. The GraphQL API is asked every 30 minutes.
Code: `src/services/GitHubService.cpp` (download), `GitHubParser.cpp` (answer), `src/app/GitHubData.h`, `drawGitHub()` in `src/ui/pages/WidgetPage.cpp`.

The whole screen is a 4 × 3 grid of widgets (no clock header or hint footer), with lines between them like on Home.
Widgets are placed in order in the first free spot, so the ones that don't fit are shown on the phone as "doesn't fit".
Code: `src/app/WidgetLayout.*` (layout), `src/ui/pages/WidgetPage.cpp` (drawing), `handleWidgets()` in `src/services/SetupPortal.cpp` (editor).

## Calendar

The device reads any calendar that has an iCal (`.ics` / `webcal://`) link: Tuudo, Google, Outlook, Apple.

1. Get the link. Tuudo: the calendar sync link. Google: Settings → your calendar → "Secret address in iCal format".
2. Paste it on the settings page (`desk.local`). Keep it private: anyone with the link can read the calendar.
   After saving, the page only shows the link's site; leave the field empty to keep it.
   `CALENDAR_ICS_URL` in `.env.local` is only used until a link is saved there.
3. The calendar is downloaded every 30 minutes, and when you hold the knob on the Home page.

Open it from the menu → Calendar. The day view shows the month with a dot on days that have events, and the events of the chosen day;
the week view shows seven columns. Turn the knob for the next or previous day (week in the week view), press to switch between the two views,
hold to go back to Home.

The parser (`src/services/IcsParser.cpp`, download in `CalendarService.cpp`) keeps events from this week's Monday to 8 weeks ahead (at most 200); the weeks after this one are used for the next event on Home.
It handles recurring events, excluded dates, moved or cancelled occurrences and all-day events.
If a feed has more than that (or more than 48 recurring events), the days that may be missing events show "Too many events" instead of looking free.
Times with a time zone are read as Finnish time, and titles are cut before course codes ("IoT Project 1 IT24SP (IT00BD93-3002)/ ..." becomes "IoT Project 1 IT24SP").

## Electricity price

Finnish spot prices from the free [spot-hinta.fi](https://spot-hinta.fi) API (no key, no setup).
The prices include VAT (25.5 %) and are shown in c/kWh; the price "now" is the current 15-minute price.

Open it from the menu → Electricity (the menu line shows the price now). The page shows the price now, the day's average,
an hourly chart (▼ = this hour) and the cheapest 3 hours in a row, filled black in the chart (for today, counted from now).
Turn the knob for tomorrow: the prices for tomorrow come out in the afternoon (about 14:00).
Press opens the menu, hold goes back to Home. It can also be added as a widget (see above).

The API allows only one request a minute per IP; too many give HTTP 429 and can block the IP for a while.
So the device asks rarely (`updatePrices()` in `src/main.cpp`): every 6 hours, every 30 minutes after 14:00 until tomorrow's prices are there,
every 5 minutes while today's prices are missing, and never more often than every 2 minutes (also when you hold to refresh).
Code: `src/services/ElectricityService.*` (download), `src/app/ElectricityData.h` (price now, daily stats, cheapest hours),
`src/ui/pages/ElectricityPage.cpp` (page).

## E-Ink display driver

Panel: ZJY800480-075AABMFGN-R (7.5", 800×480, black/white/red, UC8179 controller).
Driver: `src/display/` — `EinkDisplay` (refresh), `EinkLuts.h` (waveforms), `EinkBenchmark` (timing test).
Register values follow the panel datasheet (C22259-001 v1.3); page numbers are in the code comments.

### Switching modes

Set the flags in `platformio.ini`, then flash as usual:

| Flag | Values |
| --- | --- |
| `EPD_MODE` | `0` stock black/white/red (~29 s) · `1` black/white with the panel's own waveform · `2` black/white with our fast waveforms (default) |
| `EPD_WAVEFORM` | Mode 2: `0` normal waveforms, `1` fast set (~40% quicker, fainter, more ghosting) |
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
- It's too slow: switch to the fast set (`EPD_WAVEFORM=1`), make the phases shorter, or raise `EPD_FRAME_RATE`.
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
