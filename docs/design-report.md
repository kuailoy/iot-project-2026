## 1. Concept & Design Goals

### What the product is

A small, attractive desktop IoT device that displays environmental information and useful daily information on a low-power E-Ink display. It shows weather, temperature, humidity, optional air quality, calendar events, tasks, habits, and optionally Spotify album artwork.

### Why we chose this project

Phones and tablets provide information, but they also create distraction. Users often unlock a phone just to check the weather or next task, then end up scrolling. Information is also fragmented across apps. This project explores how a calm, always-visible desktop device can reduce screen use while keeping essential information accessible.

### What we want to achieve

- **At a glance** – See important information without checking another device.
- **Low-power** – Keep information visible with minimal energy consumption.
- **Ambient** – Provide useful information without actively demanding attention.

---

## 2. Requirements

### Functional requirements

- Display current time and date.
- Display weather forecast.
- Display indoor temperature and humidity.
- Display calendar events and tasks.
- Display habit reminders.
- Connect over Wi-Fi.
- Support configuration through a web portal.
- Support OTA firmware updates.
- Show battery status.
- Wake periodically and update the display.
- Optional: display air-quality information.
- Optional: display currently playing Spotify track and album artwork.

### Non-functional requirements

- Low power consumption using deep sleep and E-Ink persistence.
- Refresh rate suitable for E-Ink limitations.
- High contrast and glanceable layout.
- Privacy-respecting: minimal data collection, local processing where possible.
- Cost-effective components.
- Reliable operation.
- Attractive 3D-printed enclosure.
- Quiet: no sounds, no blinking, no unnecessary notifications.

---

## 3. System Design

### Main architecture

The ESP32 acts as the central controller. It reads sensors, connects to Wi-Fi, fetches data from APIs, renders the display buffer, updates the E-Ink screen, and then returns to deep sleep.

### Main parts

- **Controller:** Raspberry Pi Zero 2 W
- **Display:** Adafruit 7.5" 800×480 Monochrome E-Ink — Product 6396
- **Display Alternative:** Adafruit 5.83" 648×480 Monochrome E-Ink — Product 6397
- **Display driver:** Adafruit E-Ink Bonnet for Raspberry Pi — Product 6418
- **Sensors:** Temperature/humidity sensor, optional air-quality sensor
- **Input:** Rotary encoder with push button, optional buttons
- **Storage:** MicroSD Card
- **Power:** 3.7V Li-Po battery, charging management module, 5V boost converter
- **Enclosure:** 3D-printed enclosure

### Data flow

1. Device wakes from deep sleep on a timer or button press.
2. Reads temperature, humidity, and optional air quality.
3. Connects to Wi-Fi.
4. Fetches weather, calendar, and task data from APIs.
5. Optional: fetches current Spotify track from a small backend.
6. Renders the information into a 1-bit E-Ink frame buffer.
7. Updates the E-Ink display.
8. Turns off Wi-Fi and returns to deep sleep.

### Communication

- HTTPS and JSON for API requests.
- Optional MQTT for future smart-home integration.
- Optional local backend for OAuth, API keys, and image conversion.

### Backend role

A small backend can handle Spotify OAuth, fetch album artwork, convert it to a 1-bit bitmap, and serve it to the ESP32. This keeps the firmware simple and reduces processing load on the device.

---

## 4. Hardware & Software

### Hardware components

| Part | Choice | Notes |
|---|---|---|
| MCU | ESP32-S3 or ESP32-C6 | Wi-Fi, BLE, enough RAM for display buffer |
| Display | 5.83" 648×480 mono E-Ink | Good size, readable, low power |
| Display driver | Adafruit E-Ink Bonnet or custom driver | Supports UC8179 |
| Temp/Humidity | SHT31 or similar | Accurate and easy to use |
| Air quality | SGP30 or similar | Optional |
| Input | Rotary encoder + push button | Mode switching and refresh |
| Power | 3.7V Li-Po + charger + 5V boost | Deep sleep for long battery life |
| Enclosure | 3D-printed case | Desk stand, accessible ports |

### Software stack

- **Firmware:** Arduino / PlatformIO or ESP-IDF
- **Display library:** GxEPD2 or similar
- **Graphics:** Adafruit GFX
- **JSON:** ArduinoJson
- **Wi-Fi config:** WiFiManager
- **HTTP:** HTTPClient
- **Backend:** Python or Node.js for OAuth and image dithering
- **APIs:** Weather API, Google Calendar, Todoist, Spotify
- **Power management:** Deep sleep, timed wake, Wi-Fi off when idle

### Important design choices

- Monochrome E-Ink is chosen over color because it refreshes faster and suits dynamic information.
- ESP32 is chosen over Raspberry Pi for lower power consumption and faster wake from sleep.
- Spotify is treated as an optional showcase feature, not a core function.
- The backend handles complex tasks so the device remains low-power and reliable.

---

## 5. UI / Interaction

### Information layout

The E-Ink screen is divided into simple zones:

- **Header:** Time, date, battery, Wi-Fi status.
- **Main area:** Current mode content.
- **Footer:** Next event or a short hint.

### Modes

1. **Dashboard** – Weather, indoor temperature/humidity, next task.
2. **Calendar** – Today and upcoming events.
3. **Environment** – Temperature, humidity, optional air quality.
4. **Spotify** – Album artwork, song title, artist, playback status (not real-time).

### Interaction

- **Rotate encoder:** Switch between modes.
- **Press encoder:** Refresh data.
- **Long press encoder:** Enter configuration mode.
- **Web portal:** Set location, API keys, refresh interval, and display preferences.

### Design principles

- No blinking, no sounds, no demanding notifications.
- Slow, calm updates.
- High contrast for readability.
- Glanceable information: the user should understand the screen in a few seconds.
- The device should feel like a quiet desk object, not another screen.
