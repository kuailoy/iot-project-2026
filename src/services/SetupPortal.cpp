#include "SetupPortal.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_random.h>
#include <qrcode.h>

#include "../app/WidgetLayout.h"
#include "WeatherService.h"

namespace {

const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint8_t AP_CHANNEL = 1;
constexpr unsigned long CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long SHOW_RESULT_MS = 10000;  // Phone can show "Connected" before the setup Wi-Fi goes

String htmlEscape(const String& text) {
    String out;
    out.reserve(text.length() + 8);
    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

// 16 hex digits, new at every start
String makeToken() {
    char text[17];
    snprintf(text, sizeof(text), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    return text;
}

// The calendar is downloaded over HTTPS; webcal:// is the same
bool isCalendarLink(const String& link) {
    return link.startsWith("webcal://") || link.startsWith("https://");
}

// "https://calendar.google.com/calendar/ical/.../basic.ics" -> "calendar.google.com".
// The rest of the link is the secret part, so pages only show this.
String linkHost(const String& link) {
    int start = link.indexOf("://");
    start = start < 0 ? 0 : start + 3;
    int end = link.indexOf('/', start);
    String host = link.substring(start, end < 0 ? link.length() : end);
    int at = host.lastIndexOf('@');  // No user name or password either
    return at < 0 ? host : host.substring(at + 1);
}

// Special characters in Wi-Fi QR codes need a backslash
String qrEscape(const String& text) {
    String out;
    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

// Everything is inline: the setup Wi-Fi has no internet for fonts or images
const char* const PAGE_STYLE =
    ":root{--fg:#111;--muted:#737373;--line:#e6e6e3;--bg:#f5f5f2;--card:#fff}"
    "@media(prefers-color-scheme:dark){:root{--fg:#f2f2f2;--muted:#999;--line:#2e2e2e;--bg:#101010;--card:#1b1b1b}}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--fg);"
    "font:16px/1.45 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;-webkit-text-size-adjust:100%}"
    "main{max-width:440px;margin:0 auto;padding:32px 18px 40px}"
    ".logo{width:46px;height:32px;border:2.5px solid var(--fg);border-radius:7px;position:relative;margin-bottom:20px}"
    ".logo:after{content:'';position:absolute;left:8px;right:14px;top:8px;height:3px;border-radius:2px;"
    "background:var(--fg);box-shadow:0 7px 0 var(--fg)}"
    "h1{font-size:27px;line-height:1.2;letter-spacing:-.02em;margin:0 0 6px}"
    ".sub{color:var(--muted);margin:0 0 28px}"
    ".head{display:flex;justify-content:space-between;align-items:baseline;margin:0 4px 8px;"
    "font-size:13px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted)}"
    ".head a{color:var(--fg);text-decoration:none;text-transform:none;letter-spacing:0;font-weight:500;font-size:15px}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;overflow:hidden;margin-bottom:24px}"
    ".net{display:flex;align-items:center;gap:12px;padding:14px 16px;border-top:1px solid var(--line);cursor:pointer}"
    ".net:first-child{border-top:0}"
    ".net input{-webkit-appearance:none;appearance:none;flex:none;width:20px;height:20px;margin:0;"
    "border:2px solid var(--line);border-radius:50%}"
    ".net input:checked{border:6px solid var(--fg)}"
    ".name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".meta{display:flex;align-items:center;gap:9px;color:var(--muted)}"
    ".bars{display:flex;align-items:flex-end;gap:2px;height:14px}"
    ".bars i{width:3px;border-radius:1px;background:var(--line)}"
    ".bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}"
    ".bars i:nth-child(3){height:10px}.bars i:nth-child(4){height:14px}"
    ".s1 i:nth-child(-n+1),.s2 i:nth-child(-n+2),.s3 i:nth-child(-n+3),.s4 i{background:var(--fg)}"
    ".field{display:block;width:100%;padding:15px 16px;border:0;background:transparent;color:var(--fg);"
    "font:inherit;outline:none}"
    ".field::placeholder{color:var(--muted)}"
    ".other{border-top:1px solid var(--line)}"
    "form:has(#o:not(:checked)) .other{display:none}"
    ".pw{display:flex;align-items:center}"
    ".pw button{background:none;border:0;color:var(--muted);font:inherit;font-size:15px;padding:0 16px;cursor:pointer}"
    ".note{color:var(--muted);font-size:13px;margin:-16px 4px 24px}"
    ".btn{display:block;width:100%;padding:17px;border:0;border-radius:12px;background:var(--fg);color:var(--bg);"
    "font:inherit;font-size:17px;font-weight:600;text-align:center;text-decoration:none;cursor:pointer}"
    ".btn:disabled{opacity:.6}"
    ".foot{color:var(--muted);font-size:13px;text-align:center;margin-top:14px}"
    ".status{text-align:center;padding-top:56px}"
    ".status .sub{margin-bottom:32px}"
    ".sub b{color:var(--fg)}"
    ".icon{width:68px;height:68px;margin:0 auto 24px;border:2.5px solid var(--fg);border-radius:50%;"
    "display:flex;align-items:center;justify-content:center}"
    ".spin{border-color:var(--line);border-top-color:var(--fg);animation:r .9s linear infinite}"
    "@keyframes r{to{transform:rotate(360deg)}}"
    ".row{display:flex;justify-content:space-between;padding:15px 16px;color:var(--fg);text-decoration:none}"
    ".row span+span{color:var(--muted)}"
    ".screen{position:relative;height:0;padding-top:60%;background:var(--card);border:1px solid var(--line);"
    "border-radius:14px;margin-bottom:14px;user-select:none;-webkit-user-select:none}"
    ".grid{position:absolute;left:2%;right:2%;top:3.3%;height:93.4%}"
    ".tile{position:absolute;display:flex;flex-direction:column;justify-content:space-between;padding:6px 8px;"
    "border:2px solid var(--fg);border-radius:8px;background:var(--card);font-size:12px;font-weight:600;"
    "line-height:1.2;overflow:hidden;cursor:grab;touch-action:none}"
    ".tile small{font-weight:500;color:var(--muted)}"
    ".tile.sel{background:var(--fg);color:var(--bg)}.tile.sel small{color:var(--bg);opacity:.7}"
    ".tile.drag{z-index:2;opacity:.9;box-shadow:0 10px 28px rgba(0,0,0,.25)}"
    ".chips{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:22px}"
    ".chip{border:1px solid var(--line);border-radius:999px;background:var(--card);color:var(--fg);"
    "padding:9px 14px;font:inherit;font-size:15px;cursor:pointer}"
    ".chip.on{background:var(--fg);border-color:var(--fg);color:var(--bg)}"
    ".chip:disabled{opacity:.35}"
    ".chip.out{border-style:dashed;color:var(--muted)}.chip.out.on{color:var(--bg)}"
    ".hint{color:var(--muted);font-size:14px;margin:0 4px 22px}"
    ".foot a{color:var(--fg)}"
    ".back{display:inline-block;color:var(--muted);text-decoration:none;margin-bottom:18px}"
    ".toast{background:var(--fg);color:var(--bg);border-radius:12px;padding:12px 16px;margin:-8px 0 24px;"
    "font-weight:600}"
    ".search{display:flex;align-items:center;border-top:1px solid var(--line)}"
    ".search button{background:none;border:0;color:var(--fg);font:inherit;font-weight:600;padding:0 16px;"
    "cursor:pointer}"
    "[data-k]{display:none}";

// Small inline icons
const char* const ICON_LOCK =
    "<svg width=12 height=14 viewBox='0 0 12 14'><rect x=1 y=6 width=10 height=8 rx=2 fill=currentColor />"
    "<path d='M3.5 6V4.2a2.5 2.5 0 0 1 5 0V6' stroke=currentColor stroke-width=1.6 fill=none /></svg>";
const char* const ICON_CHECK =
    "<svg width=30 height=30 viewBox='0 0 24 24' fill=none stroke=currentColor stroke-width=2.4 "
    "stroke-linecap=round stroke-linejoin=round><path d='M5 12.5l4.5 4.5L19 7.5'/></svg>";
const char* const ICON_CROSS =
    "<svg width=26 height=26 viewBox='0 0 24 24' fill=none stroke=currentColor stroke-width=2.4 "
    "stroke-linecap=round><path d='M6 6l12 12M18 6L6 18'/></svg>";

}  // namespace

// ---------- Start / stop ----------

void SetupPortal::begin(Settings& s) {
    settings = &s;
    if (formToken.length() == 0) {
        formToken = makeToken();
    }

    // The home-network server restarts below on the setup Wi-Fi
    if (lanOn) {
        server.stop();
        MDNS.end();
        lanOn = false;
    }

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char name[24];
    snprintf(name, sizeof(name), "Desk-Setup-%02X%02X", mac[4], mac[5]);
    apName = name;

    // Stop the old home Wi-Fi from reconnecting: while it tries, the
    // ESP32 refuses to scan
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(100);

    // Look for networks before the setup Wi-Fi is on: that works best
    scan(false);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str(), settings->apPassword.c_str(), AP_CHANNEL);

    // Every name points to the device: phones then open the setup page
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", AP_IP);

    addRoutes();
    server.begin();

    setState(State::WAITING);
    Serial.printf("Setup: Wi-Fi \"%s\" is on, page at http://%s\n", apName.c_str(), AP_IP.toString().c_str());
}

void SetupPortal::addRoutes() {
    if (routesAdded) {
        return;
    }
    // Read by handleClaude()
    static const char* headers[] = {"Origin", "Content-Type"};
    server.collectHeaders(headers, 2);

    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/status", HTTP_GET, [this]() { handleStatus(); });
    server.on("/scan", HTTP_GET, [this]() { handleScan(); });
    server.on("/widgets", HTTP_GET, [this]() { handleWidgets(); });
    server.on("/widgets", HTTP_POST, [this]() { handleWidgetsSave(); });
    server.on("/settings", HTTP_POST, [this]() { handleSettingsSave(); });
    server.on("/city", HTTP_GET, [this]() { handleCity(); });
    server.on("/wifi", HTTP_GET, [this]() { handleWifiPage(); });
    server.on("/wifi", HTTP_POST, [this]() { handleWifiSave(); });
    server.on("/wifistatus", HTTP_GET, [this]() { handleWifiStatus(); });
    server.on("/claude", HTTP_POST, [this]() { handleClaude(); });
    server.on("/claude", HTTP_OPTIONS, [this]() { handleExtensionPreflight(); });
    server.on("/music", HTTP_POST, [this]() { handleMusic(); });
    server.on("/music", HTTP_OPTIONS, [this]() { handleExtensionPreflight(); });
    // Phones check e.g. /generate_204 or /hotspot-detect.html: send them to the page
    server.onNotFound([this]() { redirectToRoot(); });
    routesAdded = true;
}

void SetupPortal::startLan(Settings& s) {
    if (lanOn || state != State::OFF) {
        return;
    }
    settings = &s;
    if (formToken.length() == 0) {
        formToken = makeToken();
    }
    addRoutes();
    server.begin();
    if (MDNS.begin("desk")) {
        MDNS.addService("http", "tcp", 80);
    }
    lanOn = true;
    Serial.printf("Settings page: http://desk.local or http://%s\n", WiFi.localIP().toString().c_str());
}

static bool takeFlag(bool& flag) {
    bool result = flag;
    flag = false;
    return result;
}

bool SetupPortal::takeWidgetsChanged() { return takeFlag(widgetsChanged); }
bool SetupPortal::takeCityChanged() { return takeFlag(cityChanged); }
bool SetupPortal::takeCalendarChanged() { return takeFlag(calendarChanged); }
bool SetupPortal::takeGithubChanged() { return takeFlag(githubChanged); }
bool SetupPortal::takeWifiChanged() { return takeFlag(wifiChanged); }

bool SetupPortal::takeClaudeUsage(ClaudeUsage& usage) {
    if (!claudeReceived) {
        return false;
    }
    usage = claudeUsage;
    claudeReceived = false;
    return true;
}

bool SetupPortal::takeNowPlaying(NowPlaying& song) {
    if (!nowPlayingReceived) {
        return false;
    }
    song = nowPlaying;
    nowPlayingReceived = false;
    return true;
}

void SetupPortal::restartMdns() {
    MDNS.end();
    if (MDNS.begin("desk")) {
        MDNS.addService("http", "tcp", 80);
    }
}

void SetupPortal::end() {
    if (state == State::OFF) {
        return;
    }
    server.stop();
    dns.stop();
    WiFi.scanDelete();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    setState(State::OFF);
    Serial.println("Setup: finished");
}

bool SetupPortal::takeChanged() {
    bool result = changed;
    changed = false;
    return result;
}

bool SetupPortal::finished() const {
    return state == State::CONNECTED && millis() - connectedAt >= SHOW_RESULT_MS;
}

void SetupPortal::setState(State next) {
    if (next != state) {
        state = next;
        changed = true;
    }
}

String SetupPortal::statusText() const {
    switch (state) {
        case State::WAITING:
            return "Waiting for a phone to connect...";
        case State::PHONE_JOINED:
            return "Phone connected, open the setup page";
        case State::CONNECTING:
            return "Connecting to " + pendingSsid + "...";
        case State::CONNECTED:
            return "Connected to " + pendingSsid;
        case State::FAILED:
            return "Could not connect: " + failReason;
        default:
            return "";
    }
}

// ---------- Loop ----------

void SetupPortal::loop() {
    if (state == State::OFF) {
        if (lanOn) {
            server.handleClient();  // Settings pages on the home Wi-Fi
            loopSwitch();
        }
        return;
    }
    dns.processNextRequest();
    server.handleClient();

    switch (state) {
        case State::WAITING:
        case State::PHONE_JOINED:
            setState(WiFi.softAPgetStationNum() > 0 ? State::PHONE_JOINED : State::WAITING);
            break;

        case State::CONNECTING: {
            wl_status_t status = WiFi.status();
            unsigned long elapsed = millis() - connectStart;
            if (status == WL_CONNECTED) {
                settings->wifiSsid = pendingSsid;
                settings->wifiPassword = pendingPassword;
                SettingsStore::save(*settings);
                connectedAt = millis();
                setState(State::CONNECTED);
                Serial.printf("Setup: connected to %s, settings saved\n", pendingSsid.c_str());
            } else if (elapsed > CONNECT_TIMEOUT_MS ||
                       (elapsed > 4000 && (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL))) {
                failReason = status == WL_NO_SSID_AVAIL ? "network not found" : "check the password";
                WiFi.disconnect();
                setState(State::FAILED);
                Serial.printf("Setup: could not connect to %s (status %d)\n", pendingSsid.c_str(), status);
            }
            break;
        }

        default:
            break;
    }
}

// Switching the home Wi-Fi from the settings page: try the new network,
// go back to the old one if it doesn't work
void SetupPortal::loopSwitch() {
    switch (switchState) {
        case Switch::PENDING:
            // Give the answer page time to reach the phone before leaving the network
            if (millis() - switchStart > 800) {
                Serial.printf("Wi-Fi: switching to %s\n", switchSsid.c_str());
                WiFi.disconnect();
                WiFi.begin(switchSsid.c_str(), switchPassword.c_str());
                switchStart = millis();
                switchState = Switch::SWITCHING;
            }
            break;

        case Switch::SWITCHING: {
            wl_status_t status = WiFi.status();
            unsigned long elapsed = millis() - switchStart;
            if (status == WL_CONNECTED) {
                settings->wifiSsid = switchSsid;
                settings->wifiPassword = switchPassword;
                SettingsStore::save(*settings);
                switchState = Switch::DONE;
                wifiChanged = true;
                mdnsRestart = true;
                Serial.printf("Wi-Fi: now on %s, saved\n", switchSsid.c_str());
            } else if (elapsed > CONNECT_TIMEOUT_MS ||
                       (elapsed > 4000 && (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL))) {
                Serial.printf("Wi-Fi: %s didn't work (status %d), back to %s\n", switchSsid.c_str(), status,
                              oldSsid.c_str());
                WiFi.disconnect();
                WiFi.begin(oldSsid.c_str(), oldPassword.c_str());
                switchState = Switch::FAILED;
                mdnsRestart = true;
            }
            break;
        }

        default:
            break;
    }

    // desk.local has to be announced again on the (new) network
    if (mdnsRestart && WiFi.status() == WL_CONNECTED) {
        restartMdns();
        mdnsRestart = false;
    }
}

// ---------- Wi-Fi scan ----------

// Look for networks and wait for the result (2-4 s)
void SetupPortal::scan(bool onHomeWifi) {
    unsigned long start = millis();
    WiFi.scanDelete();
    int16_t found = WiFi.scanNetworks(false, false, false, 300);
    if (found < 0) {
        // Usually busy with a connection attempt. During setup it can be
        // stopped; on the home Wi-Fi that would drop the page being served.
        Serial.printf("Setup: Wi-Fi scan failed (%d), trying again\n", found);
        if (!onHomeWifi) {
            WiFi.disconnect();
        }
        delay(200);
        found = WiFi.scanNetworks(false, false, false, 300);
    }
    if (found < 0) {
        Serial.printf("Setup: Wi-Fi scan failed again (%d)\n", found);
        WiFi.scanDelete();
        return;  // Keep the old list
    }

    // One entry per name, the strongest signal, strongest first
    networkCount = 0;
    for (int i = 0; i < found; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        if (ssid.length() == 0) {
            continue;  // Hidden network
        }
        int existing = -1;
        for (int j = 0; j < networkCount; j++) {
            if (networks[j].ssid == ssid) {
                existing = j;
                break;
            }
        }
        if (existing >= 0) {
            if (rssi > networks[existing].rssi) {
                networks[existing].rssi = rssi;
            }
            continue;
        }
        if (networkCount < MAX_NETWORKS) {
            networks[networkCount++] = {ssid, rssi, WiFi.encryptionType(i) == WIFI_AUTH_OPEN};
        }
    }
    for (int i = 1; i < networkCount; i++) {
        for (int j = i; j > 0 && networks[j].rssi > networks[j - 1].rssi; j--) {
            Network tmp = networks[j];
            networks[j] = networks[j - 1];
            networks[j - 1] = tmp;
        }
    }
    WiFi.scanDelete();
    Serial.printf("Setup: found %d networks in %lu ms\n", networkCount, millis() - start);
}

// ---------- Web pages ----------

void SetupPortal::sendPage(const String& body, int refreshSeconds) {
    String html;
    html.reserve(body.length() + 3600);
    html += "<!DOCTYPE html><html><head><meta charset=utf-8>"
            "<meta name=viewport content=\"width=device-width,initial-scale=1\">";
    if (refreshSeconds > 0) {
        html += "<meta http-equiv=refresh content=\"" + String(refreshSeconds) + ";url=/status\">";
    }
    html += "<title>Desk setup</title><style>";
    html += PAGE_STYLE;
    html += "</style></head><body><main>";
    html += body;
    html += "</main></body></html>";
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Options", "DENY");  // Other sites can't show the forms inside theirs
    server.send(200, "text/html", html);
}

String SetupPortal::tokenField() const {
    return "<input type=hidden name=t value=" + formToken + ">";
}

bool SetupPortal::validToken() {
    return formToken.length() > 0 && server.arg("t") == formToken;
}

// After a restart the token is new, so an old page lands here too
void SetupPortal::sendExpired() {
    Serial.println("Web: form without a valid token, ignored");
    sendStatusPage(ICON_CROSS, "Page expired", "Open the page again and try once more.", "Back");
}

// Centered result page: icon, title, text and an optional button
void SetupPortal::sendStatusPage(const char* icon, const String& title, const String& text,
                                 const String& button, int refreshSeconds) {
    String body = "<div class=status><div class=\"icon";
    body += icon == nullptr ? " spin\">" : "\">";
    if (icon != nullptr) {
        body += icon;
    }
    body += "</div><h1>" + title + "</h1><p class=sub>" + text + "</p>";
    if (button.length() > 0) {
        body += "<a class=btn href=/>" + button + "</a>";
    }
    body += "</div>";
    sendPage(body, refreshSeconds);
}

void SetupPortal::redirectToRoot() {
    String location = state != State::OFF ? "http://" + AP_IP.toString() + "/" : String("/");
    server.sendHeader("Location", location, true);
    server.send(302, "text/plain", "");
}

void SetupPortal::handleRoot() {
    if (state == State::OFF) {
        handleSettings();  // On the home Wi-Fi: http://desk.local
        return;
    }

    // First setup: Wi-Fi only, everything else is on desk.local afterwards
    String body;
    body.reserve(2000 + networkCount * 420);
    body += "<div class=logo></div><h1>Desk setup</h1><p class=sub>Connect your display to Wi-Fi.</p>";
    body += "<form method=post action=/save onsubmit=\"b.disabled=true;b.textContent='Connecting...'\">";
    body += tokenField();
    // Selected: the saved network, or the strongest one
    String preselect = settings->hasWifi() || networkCount == 0 ? settings->wifiSsid : networks[0].ssid;
    body += wifiFields(preselect, "/scan");
    body += "<button class=btn id=b>Connect</button>"
            "<p class=foot>Only 2.4 GHz networks are supported</p></form>";
    sendPage(body);
}

String SetupPortal::wifiFields(const String& preselect, const char* refreshLink) {
    String body;
    body.reserve(1400 + networkCount * 420);

    // Networks: one row each, strongest first, then "Other network"
    body += "<div class=head><span>Wi-Fi network</span><a href=";
    body += refreshLink;
    body += ">Refresh</a></div><div class=card>";
    bool anyChecked = false;
    auto addRow = [&](const String& ssid, int rssi, bool open, bool known) {
        bool checked = !anyChecked && ssid == preselect;
        anyChecked |= checked;
        int level = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
        body += "<label class=net><input type=radio name=ssid value=\"" + htmlEscape(ssid) + "\"";
        body += checked ? " checked>" : ">";
        body += "<span class=name>" + htmlEscape(ssid) + "</span><span class=meta>";
        if (known) {
            if (!open) {
                body += ICON_LOCK;
            }
            body += "<span class=\"bars s" + String(level) + "\"><i></i><i></i><i></i><i></i></span>";
        } else {
            body += "not found";
        }
        body += "</span></label>";
    };
    bool savedListed = false;
    for (int i = 0; i < networkCount; i++) {
        addRow(networks[i].ssid, networks[i].rssi, networks[i].open, true);
        savedListed |= networks[i].ssid == settings->wifiSsid;
    }
    if (settings->hasWifi() && !savedListed) {
        addRow(settings->wifiSsid, 0, false, false);
    }
    body += "<label class=net><input type=radio name=ssid value=\"\" id=o";
    body += anyChecked ? ">" : " checked>";
    body += "<span class=name>Other network...</span></label>";
    body += "<input class=\"field other\" name=other placeholder=\"Network name\" "
            "autocapitalize=none autocorrect=off spellcheck=false></div>";
    if (networkCount == 0) {
        body += "<p class=note>No networks found. Tap Refresh to search again.</p>";
    }

    // Password with a show / hide button
    body += "<div class=head><span>Password</span></div>"
            "<div class=\"card pw\"><input class=field id=p name=pass type=password "
            "autocapitalize=none autocorrect=off spellcheck=false placeholder=\"";
    body += settings->hasWifi() ? "Leave empty to keep the saved one" : "Leave empty for an open network";
    body += "\"><button type=button onclick=\"var h=p.type=='password';p.type=h?'text':'password';"
            "this.textContent=h?'Hide':'Show'\">Show</button></div>";
    return body;
}

bool SetupPortal::readWifiForm(String& ssid, String& password) {
    // A network from the list, or "Other network" with a typed name
    ssid = server.arg("ssid");
    if (ssid.length() == 0) {
        ssid = server.arg("other");
        ssid.trim();
    }
    password = server.arg("pass");

    if (ssid.length() == 0) {
        sendStatusPage(ICON_CROSS, "Choose a network", "Pick one from the list or type its name.", "Back");
        return false;
    }
    // Same network and no new password: keep the saved one
    if (password.length() == 0 && ssid == settings->wifiSsid) {
        password = settings->wifiPassword;
    }
    if (password.length() > 0 && password.length() < 8) {
        sendStatusPage(ICON_CROSS, "Password too short", "Wi-Fi passwords have at least 8 characters.", "Back");
        return false;
    }
    return true;
}

void SetupPortal::handleScan() {
    if (state == State::OFF) {
        redirectToRoot();
        return;
    }
    if (state != State::CONNECTING) {
        scan(false);  // The page waits a few seconds for the result
    }
    redirectToRoot();
}

void SetupPortal::handleSave() {
    if (state == State::OFF) {
        redirectToRoot();  // At home the Wi-Fi is changed on /wifi
        return;
    }
    if (!validToken()) {
        sendExpired();
        return;
    }
    String ssid, password;
    if (!readWifiForm(ssid, password)) {
        return;
    }

    pendingSsid = ssid;
    pendingPassword = password;
    connectStart = millis();
    WiFi.disconnect();
    WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
    setState(State::CONNECTING);

    handleStatus();
}

void SetupPortal::handleStatus() {
    String name = "<b>" + htmlEscape(pendingSsid) + "</b>";
    switch (state) {
        case State::CONNECTING:
            sendStatusPage(nullptr, "Connecting...",
                           "Joining " + name + ". This takes up to 20 seconds. Your phone may drop the setup "
                           "Wi-Fi for a moment; the display shows the result.",
                           "", 2);
            return;
        case State::CONNECTED:
            sendStatusPage(ICON_CHECK, "All set",
                           "Connected to " + name + ". Join " + name + " with your phone and open "
                           "<b>desk.local</b> for widgets, weather city and calendar.",
                           "");
            return;
        case State::FAILED:
            sendStatusPage(ICON_CROSS, "Couldn't connect", name + ": " + htmlEscape(failReason) + ".",
                           "Try again");
            return;
        default:
            redirectToRoot();
            return;
    }
}

// ---------- Widget editor (second home page) ----------

namespace {

// Phone-side editor: preview in the device's proportions, drag to reorder,
// tap to select, size / move / remove buttons. Placement is the same as in
// WidgetLayout.cpp (first free spot, row by row, 4 x 3 cells).
const char* const EDITOR_SCRIPT = R"JS(
var SZ={S:[1,1],M:[2,1],L:[2,2],W:[4,1]},SN={S:"Small",M:"Medium",L:"Large",W:"Wide"};
var L=parse(CUR),sel=-1,dirty=false;
function $(i){return document.getElementById(i)}
function name(c){for(var i=0;i<CAT.length;i++)if(CAT[i][0]==c)return CAT[i][1];return""}
function parse(t){var a=[];t.split(",").forEach(function(p){var k=p.split(":");
 if(k.length==2&&name(k[0])&&SZ[k[1]])a.push({t:k[0],s:k[1]})});return a.slice(0,12)}
function ser(){return L.map(function(w){return w.t+":"+w.s}).join(",")}
function place(){var u=[[0,0,0,0],[0,0,0,0],[0,0,0,0]];L.forEach(function(w){var d=SZ[w.s];w.p=null;
 for(var r=0;r+d[1]<=3&&!w.p;r++)for(var c=0;c+d[0]<=4&&!w.p;c++){var ok=1,x,y;
  for(y=r;y<r+d[1];y++)for(x=c;x<c+d[0];x++)if(u[y][x])ok=0;
  if(ok){for(y=r;y<r+d[1];y++)for(x=c;x<c+d[0];x++)u[y][x]=1;w.p=[c,r]}}})}
function draw(){place();var s=$("scr"),o=$("out");s.innerHTML="";o.innerHTML="";
 L.forEach(function(w,i){var d=SZ[w.s];
  if(w.p){var t=document.createElement("div");
   t.className="tile"+(i==sel?" sel":"");t.dataset.i=i;
   t.style.left="calc("+w.p[0]*25+"% + 3px)";t.style.top="calc("+w.p[1]*100/3+"% + 3px)";
   t.style.width="calc("+d[0]*25+"% - 6px)";t.style.height="calc("+d[1]*100/3+"% - 6px)";
   t.innerHTML=name(w.t)+"<small>"+SN[w.s]+"</small>";grab(t);s.appendChild(t)}
  else{var b=document.createElement("button");b.type="button";b.className="chip out"+(i==sel?" on":"");
   b.textContent=name(w.t)+" · doesn't fit";b.onclick=function(){sel=i;draw()};o.appendChild(b)}});
 o.style.display=o.children.length?"":"none";
 $("tools").style.display=sel>=0?"":"none";$("hint").style.display=sel>=0?"none":"";
 if(sel>=0){$("tname").textContent=name(L[sel].t);
  [].forEach.call(document.querySelectorAll("[data-s]"),function(b){b.className="chip"+(b.getAttribute("data-s")==L[sel].s?" on":"")});
  $("up").disabled=sel==0;$("dn").disabled=sel==L.length-1}
 [].forEach.call(document.querySelectorAll("[data-a]"),function(b){b.disabled=L.length>=12});
 $("cnt").textContent=L.length+" of 12";
 var v=$("save");v.disabled=!dirty;v.textContent=dirty?"Save to display":"Saved"}
function change(){dirty=true;draw()}
function size(z){L[sel].s=z;change()}
function move(d){var w=L.splice(sel,1)[0];sel+=d;L.splice(sel,0,w);change()}
function del(){L.splice(sel,1);sel=-1;change()}
function add(c){if(L.length>=12)return;L.push({t:c,s:"S"});sel=L.length-1;change()}
function reset(){L=parse(DEF);sel=-1;change()}
function grab(t){t.onpointerdown=function(e){var i=+t.dataset.i,x0=e.clientX,y0=e.clientY,moved=false;
 try{t.setPointerCapture(e.pointerId)}catch(x){}
 t.onpointermove=function(e){var dx=e.clientX-x0,dy=e.clientY-y0;
  if(!moved&&Math.abs(dx)+Math.abs(dy)<8)return;moved=true;t.className="tile drag sel";
  t.style.transform="translate("+dx+"px,"+dy+"px)"};
 t.onpointerup=function(e){t.onpointermove=t.onpointerup=null;
  if(!moved){sel=i;draw();return}
  t.style.visibility="hidden";var h=document.elementFromPoint(e.clientX,e.clientY);t.style.visibility="";
  var on=h&&h.closest?h.closest(".tile"):null,j=on?+on.dataset.i:L.length-1;
  var w=L.splice(i,1)[0];L.splice(j,0,w);sel=j;change()}}}
function save(){var v=$("save");v.disabled=true;v.textContent="Saving...";var x=new XMLHttpRequest();
 x.open("POST","/widgets");x.setRequestHeader("Content-Type","application/x-www-form-urlencoded");
 x.onload=function(){if(x.status==200){dirty=false;draw()}else{v.disabled=false;v.textContent="Try again"}};
 x.onerror=function(){v.disabled=false;v.textContent="Try again"};
 x.send("layout="+encodeURIComponent(ser())+"&t="+TOK)}
draw();
)JS";

}  // namespace

void SetupPortal::handleWidgets() {
    String body;
    body.reserve(5200);
    body += "<a class=back href=/>&lsaquo; Settings</a><h1>Second screen</h1>"
            "<p class=sub>Opens with the touch button. Drag widgets to move them, tap one to change its size.</p>";

    body += "<div class=head><span>Display</span><span id=cnt></span></div>"
            "<div class=screen><div class=grid id=scr></div></div><div class=chips id=out></div>"
            "<p class=hint id=hint>Tap a widget to resize, move or remove it.</p>";

    body += "<div id=tools><div class=head><span id=tname></span></div><div class=chips>";
    const char* const SIZE_NAMES[SIZE_COUNT] = {"Small", "Medium", "Large", "Wide"};
    for (uint8_t z = 0; z < SIZE_COUNT; z++) {
        body += String("<button type=button class=chip data-s=") + sizeCode(z) + " onclick=\"size('" +
                sizeCode(z) + "')\">" + SIZE_NAMES[z] + "</button>";
    }
    body += "</div><div class=chips>"
            "<button type=button class=chip id=up onclick=move(-1)>&larr; Earlier</button>"
            "<button type=button class=chip id=dn onclick=move(1)>Later &rarr;</button>"
            "<button type=button class=chip onclick=del()>Remove</button></div></div>";

    body += "<div class=head><span>Add a widget</span></div><div class=chips>";
    for (uint8_t t = 0; t < WIDGET_TYPE_COUNT; t++) {
        body += String("<button type=button class=chip data-a onclick=\"add('") + widgetCode(t) + "')\">+ " +
                widgetName(t) + "</button>";
    }
    body += "</div>";

    body += "<button type=button class=btn id=save onclick=save()>Saved</button>"
            "<p class=foot><a href=# onclick=\"reset();return false\">Reset to default</a></p>";

    // Data for the script: widget catalog, default and current layout
    body += "<script>var CAT=[";
    for (uint8_t t = 0; t < WIDGET_TYPE_COUNT; t++) {
        body += String(t > 0 ? "," : "") + "[\"" + widgetCode(t) + "\",\"" + widgetName(t) + "\"]";
    }
    body += String("],DEF=\"") + DEFAULT_WIDGETS + "\",CUR=\"" + htmlEscape(settings->widgets) + "\",TOK=\"" +
            formToken + "\";";
    body += EDITOR_SCRIPT;
    body += "</script>";
    sendPage(body);
}

void SetupPortal::handleWidgetsSave() {
    if (!validToken()) {
        server.send(403, "text/plain", "page expired, open it again");  // The editor shows "Try again"
        return;
    }
    char layout[160];
    normalizeWidgets(server.arg("layout").c_str(), layout, sizeof(layout));  // Empty is fine: no widgets
    settings->widgets = layout;
    SettingsStore::save(*settings);
    widgetsChanged = true;
    Serial.printf("Widgets saved: %s\n", layout);
    server.send(200, "text/plain", "ok");
}

// ---------- Settings on the home Wi-Fi (desk.local) ----------

namespace {

// City search: asks the device (/city), which asks OpenWeatherMap
const char* const CITY_SCRIPT = R"JS(
function $(i){return document.getElementById(i)}
function line(t){$("res").innerHTML="<div class=row><span>"+t+"</span></div>"}
function find(){var q=$("q").value.trim();if(q.length<2)return;line("Searching...");
 var x=new XMLHttpRequest();x.open("GET","/city?q="+encodeURIComponent(q));
 x.onload=function(){var a=[];try{a=JSON.parse(x.responseText)}catch(e){}
  if(x.status!=200){line("Search didn't work, try again");return}
  if(!a.length){line("No city found");return}
  var r=$("res");r.innerHTML="";
  a.forEach(function(c,i){var l=document.createElement("label");l.className="net";
   l.innerHTML="<input type=radio name=pick><span class=name></span><span class=meta></span>";
   l.querySelector(".name").textContent=c.n;l.querySelector(".meta").textContent=c.r;
   var p=l.querySelector("input");p.onchange=function(){$("cn").value=c.n;$("la").value=c.lat;
    $("lo").value=c.lon;$("cur").textContent=c.n+(c.r?", "+c.r:"")};
   if(i==0){p.checked=true;p.onchange()}r.appendChild(l)})};
 x.onerror=function(){line("Search didn't work, try again")};x.send()}
)JS";

// Wi-Fi switch: the device may leave this network, so keep asking and
// explain what happened when it stops answering
const char* const SWITCH_SCRIPT = R"JS(
var t0=Date.now();
function show(k){[].forEach.call(document.querySelectorAll("[data-k]"),function(e){
 e.style.display=e.getAttribute("data-k")==k?"block":"none"})}
function poll(){var x=new XMLHttpRequest();x.open("GET","/wifistatus");x.timeout=2000;
 x.onload=function(){var s=x.responseText;if(s=="failed")show("f");else if(s=="done")show("d");
  else setTimeout(poll,1500)};
 x.onerror=x.ontimeout=function(){if(Date.now()-t0>40000)show("g");else setTimeout(poll,1500)};x.send()}
show("w");setTimeout(poll,3000);
)JS";

}  // namespace

void SetupPortal::handleSettings() {
    String body;
    body.reserve(5000);
    body += "<div class=logo></div><h1>Desk settings</h1>";
    body += "<p class=sub>On <b>" + htmlEscape(settings->wifiSsid) + "</b> &middot; " +
            WiFi.localIP().toString() + "</p>";
    if (server.arg("saved") == "1") {
        body += "<div class=toast>Saved</div>";
    }

    // Second screen
    int widgets = settings->widgets.length() == 0 ? 0 : 1;
    for (size_t i = 0; i < settings->widgets.length(); i++) {
        widgets += settings->widgets[i] == ',';
    }
    body += "<div class=head><span>Second screen</span></div><div class=card><a class=row href=/widgets>"
            "<span>Widgets</span><span>" + String(widgets) + (widgets == 1 ? " widget" : " widgets") +
            " &rsaquo;</span></a></div>";

    // Claude usage from the browser extension
    String claudeStatus = "Not received yet";
    if (claudeAtMs != 0) {
        unsigned long minutes = (millis() - claudeAtMs) / 60000;
        claudeStatus = String(int(claudeUsage.sessionPercent + 0.5f)) + "% session &middot; " +
                       (minutes == 0 ? String("just now") : String(minutes) + " min ago");
    }
    body += "<div class=head><span>Claude usage</span></div><div class=card><div class=row>"
            "<span>Browser extension</span><span>" + claudeStatus + "</span></div></div>"
            "<p class=note>Sent by the Chrome extension in tools/desk-extension, "
            "for the Claude widget.</p>";

    body += "<form method=post action=/settings>";
    body += tokenField();

    // Weather city: search, then pick one of the results
    body += "<div class=head><span>Weather city</span></div><div class=card>"
            "<div class=row><span id=cur>" + htmlEscape(settings->cityName) + "</span><span>Now</span></div>"
            "<div class=search><input class=field id=q placeholder=\"Search for a city\" enterkeyhint=search "
            "autocorrect=off onkeydown=\"if(event.key=='Enter'){find();return false}\">"
            "<button type=button onclick=find()>Search</button></div><div id=res></div></div>";
    body += "<input type=hidden name=city id=cn value=\"" + htmlEscape(settings->cityName) + "\">";
    body += "<input type=hidden name=lat id=la value=\"" + String(settings->cityLat, 4) + "\">";
    body += "<input type=hidden name=lon id=lo value=\"" + String(settings->cityLon, 4) + "\">";

    // Calendar link. The saved one is a secret address: only its site is shown,
    // and an empty field keeps it.
    bool hasCalendar = settings->calendarUrl.length() > 0;
    body += "<div class=head><span>Calendar link</span><span>Optional</span></div><div class=card>";
    if (hasCalendar) {
        body += "<div class=row><span>" + htmlEscape(linkHost(settings->calendarUrl)) +
                "</span><span>Saved</span></div>"
                "<label class=net><input type=checkbox name=calclear value=1>"
                "<span class=name>Remove the saved link</span></label>";
    }
    body += "<input class=field name=cal inputmode=url autocapitalize=none autocorrect=off spellcheck=false "
            "placeholder=\"";
    body += hasCalendar ? "New link, or leave empty to keep it" : "webcal://...";
    body += "\"></div>";
    body += "<p class=note>The iCal or webcal link from Tuudo, Google Calendar or Outlook.</p>";

    // GitHub token for the contribution graph widget. A secret too: never sent back.
    bool hasGithub = settings->githubToken.length() > 0;
    body += "<div class=head><span>GitHub token</span><span>Optional</span></div><div class=card>";
    if (hasGithub) {
        body += "<div class=row><span>Token</span><span>Saved</span></div>"
                "<label class=net><input type=checkbox name=ghclear value=1>"
                "<span class=name>Remove the saved token</span></label>";
    }
    body += "<input class=field name=gh type=password autocomplete=off autocapitalize=none autocorrect=off "
            "spellcheck=false placeholder=\"";
    body += hasGithub ? "New token, or leave empty to keep it" : "github_pat_...";
    body += "\"></div>";
    body += "<p class=note>For the GitHub widget. github.com &rsaquo; Settings &rsaquo; Developer settings "
            "&rsaquo; Personal access tokens: a token without any permissions is enough.</p>";
    body += "<button class=btn>Save</button></form>";

    // Wi-Fi
    body += "<div class=head style=\"margin-top:28px\"><span>Wi-Fi</span></div><div class=card>"
            "<a class=row href=/wifi><span>" + htmlEscape(settings->wifiSsid) +
            "</span><span>Change &rsaquo;</span></a></div>";

    body += "<script>";
    body += CITY_SCRIPT;
    body += "</script>";
    sendPage(body);
}

void SetupPortal::handleSettingsSave() {
    if (state != State::OFF) {
        redirectToRoot();
        return;
    }
    if (!validToken()) {
        sendExpired();
        return;
    }

    // Calendar link: a new one, removed, or kept when the field is empty
    String calendar = server.arg("cal");
    calendar.trim();
    if (server.arg("calclear") == "1") {
        calendar = "";
    } else if (calendar.length() == 0) {
        calendar = settings->calendarUrl;
    } else if (!isCalendarLink(calendar)) {
        sendStatusPage(ICON_CROSS, "Not saved", "The calendar link should start with webcal:// or https://.",
                       "Back");
        return;
    }

    // GitHub token: a new one, removed, or kept when the field is empty
    String github = server.arg("gh");
    github.trim();
    if (server.arg("ghclear") == "1") {
        github = "";
    } else if (github.length() == 0) {
        github = settings->githubToken;
    } else if (github.indexOf(' ') >= 0) {
        sendStatusPage(ICON_CROSS, "Not saved", "A GitHub token has no spaces: copy it again.", "Back");
        return;
    }

    // Weather city: only when all three fields came (toFloat() of "" is 0)
    String city = server.arg("city");
    city.trim();
    String latText = server.arg("lat");
    String lonText = server.arg("lon");
    float lat = latText.toFloat();
    float lon = lonText.toFloat();
    bool cityValid = city.length() > 0 && latText.length() > 0 && lonText.length() > 0 && lat >= -90 &&
                     lat <= 90 && lon >= -180 && lon <= 180;
    if (cityValid && (city != settings->cityName || lat != settings->cityLat || lon != settings->cityLon)) {
        settings->cityName = city;
        settings->cityLat = lat;
        settings->cityLon = lon;
        cityChanged = true;
        Serial.printf("Settings: city %s (%.4f, %.4f)\n", city.c_str(), lat, lon);
    }
    if (calendar != settings->calendarUrl) {
        settings->calendarUrl = calendar;
        calendarChanged = true;
        Serial.println("Settings: calendar link changed");
    }
    if (github != settings->githubToken) {
        settings->githubToken = github;
        githubChanged = true;
        Serial.println("Settings: GitHub token changed");
    }
    SettingsStore::save(*settings);

    server.sendHeader("Location", "/?saved=1", true);
    server.send(303, "text/plain", "");
}

void SetupPortal::handleCity() {
    String query = server.arg("q");
    query.trim();
    if (query.length() < 2) {
        server.send(200, "application/json", "[]");
        return;
    }
    String json, error;
    if (!findCities(query, weatherKey, json, error)) {
        Serial.printf("City search failed: %s\n", error.c_str());
        server.send(502, "application/json", "[]");
        return;
    }
    server.send(200, "application/json", json);
}

void SetupPortal::handleWifiPage() {
    if (state != State::OFF) {
        redirectToRoot();
        return;
    }
    scan(true);  // A few seconds; the page opens with a fresh list
    String body;
    body.reserve(2400 + networkCount * 420);
    body += "<a class=back href=/>&lsaquo; Settings</a><h1>Wi-Fi</h1>";
    body += "<p class=sub>The display is on <b>" + htmlEscape(settings->wifiSsid) + "</b>.</p>";
    body += "<form method=post action=/wifi onsubmit=\"b.disabled=true;b.textContent='Switching...'\">";
    body += tokenField();
    body += wifiFields(settings->wifiSsid, "/wifi");
    body += "<button class=btn id=b>Switch network</button>";
    body += "<p class=foot>If the new network doesn't work, the display goes back to <b>" +
            htmlEscape(settings->wifiSsid) + "</b> after about 20 seconds.</p></form>";
    sendPage(body);
}

void SetupPortal::handleWifiSave() {
    if (state != State::OFF || switchState == Switch::PENDING || switchState == Switch::SWITCHING) {
        redirectToRoot();
        return;
    }
    if (!validToken()) {
        sendExpired();
        return;
    }
    String ssid, password;
    if (!readWifiForm(ssid, password)) {
        return;
    }
    if (ssid == settings->wifiSsid && password == settings->wifiPassword) {
        redirectToRoot();  // Nothing changes
        return;
    }

    oldSsid = settings->wifiSsid;
    oldPassword = settings->wifiPassword;
    switchSsid = ssid;
    switchPassword = password;
    switchStart = millis();
    switchState = Switch::PENDING;  // Starts in loop(), after this page is sent

    String name = "<b>" + htmlEscape(ssid) + "</b>";
    String old = "<b>" + htmlEscape(oldSsid) + "</b>";
    String body = "<div class=status>";
    body += "<div data-k=w><div class=\"icon spin\"></div><h1>Switching...</h1><p class=sub>Joining " + name +
            ". This takes up to 20 seconds.</p></div>";
    body += "<div data-k=d><div class=icon>" + String(ICON_CHECK) + "</div><h1>All set</h1><p class=sub>"
            "The display is on " + name + " now.</p><a class=btn href=/>Back to settings</a></div>";
    body += "<div data-k=f><div class=icon>" + String(ICON_CROSS) + "</div><h1>Couldn't connect</h1>"
            "<p class=sub>" + name + " didn't work, so the display went back to " + old +
            ". Check the password.</p><a class=btn href=/wifi>Try again</a></div>";
    body += "<div data-k=g><div class=icon>" + String(ICON_CHECK) + "</div><h1>Display moved</h1>"
            "<p class=sub>The display is probably on " + name + " now. Connect your phone to " + name +
            " and open <b>desk.local</b>.</p></div>";
    body += "</div><script>";
    body += SWITCH_SCRIPT;
    body += "</script>";
    sendPage(body);
}

void SetupPortal::handleWifiStatus() {
    const char* text = switchState == Switch::DONE     ? "done"
                     : switchState == Switch::FAILED   ? "failed"
                     : switchState == Switch::IDLE     ? "idle"
                                                       : "switching";
    server.send(200, "text/plain", text);
}

// ---------- Claude usage (browser extension) ----------

// The extension (tools/desk-extension) runs
// in the user's browser and posts JSON. Only extensions get CORS permission, web
// pages don't: then a page can't send JSON here either, the browser asks first
// (OPTIONS) and stops.
void SetupPortal::allowExtension() {
    String origin = server.header("Origin");
    if (!origin.startsWith("chrome-extension://")) {
        return;
    }
    server.sendHeader("Access-Control-Allow-Origin", origin);
    server.sendHeader("Vary", "Origin");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void SetupPortal::handleExtensionPreflight() {
    allowExtension();
    server.send(204, "text/plain", "");
}

// A form or text/plain body could come from any web page without asking first
bool SetupPortal::requireJson() {
    if (server.header("Content-Type").startsWith("application/json")) {
        return true;
    }
    server.send(415, "text/plain", "send JSON (Content-Type: application/json)");
    return false;
}

void SetupPortal::handleClaude() {
    allowExtension();
    if (!requireJson()) {
        return;
    }
    ClaudeUsage usage;
    if (!parseClaudeUsage(server.arg("plain").c_str(), usage)) {
        server.send(400, "text/plain", "expected {\"session\":54,\"weekly\":29,...}");
        return;
    }
    claudeUsage = usage;
    claudeReceived = true;
    claudeAtMs = millis();
    if (claudeAtMs == 0) {
        claudeAtMs = 1;
    }
    server.send(200, "text/plain", "ok");
}

// ---------- Now playing (browser extension) ----------

void SetupPortal::handleMusic() {
    allowExtension();
    if (!requireJson()) {
        return;
    }
    NowPlaying song;
    if (!parseNowPlaying(server.arg("plain").c_str(), song)) {
        server.send(400, "text/plain", "expected {\"title\":\"...\",\"artist\":\"...\",\"playing\":true,...}");
        return;
    }
    nowPlaying = song;
    nowPlayingReceived = true;
    server.send(200, "text/plain", "ok");
}

// ---------- QR code ----------

namespace {

uint8_t* qrBits = nullptr;
size_t qrBitsSize = 0;
int qrSize = 0;

// Called by esp_qrcode_generate with the finished code
void storeQrCode(esp_qrcode_handle_t code) {
    int size = esp_qrcode_get_size(code);
    if (size_t(size * size + 7) / 8 > qrBitsSize) {
        qrSize = 0;
        return;
    }
    memset(qrBits, 0, qrBitsSize);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(code, x, y)) {
                int i = y * size + x;
                qrBits[i / 8] |= 0x80 >> (i % 8);
            }
        }
    }
    qrSize = size;
}

}  // namespace

int makeWifiQrCode(const String& ssid, const String& password, uint8_t* bits, size_t bitsSize) {
    return makeQrCode("WIFI:T:WPA;S:" + qrEscape(ssid) + ";P:" + qrEscape(password) + ";;", bits, bitsSize);
}

int makeQrCode(const String& text, uint8_t* bits, size_t bitsSize) {
    qrBits = bits;
    qrBitsSize = bitsSize;
    qrSize = 0;

    esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
    config.display_func = storeQrCode;
    config.max_qrcode_version = 6;  // Up to 41 x 41 modules
    config.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
    if (esp_qrcode_generate(&config, text.c_str()) != ESP_OK) {
        return 0;
    }
    return qrSize;
}
