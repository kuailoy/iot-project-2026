#include "NowPlaying.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../app/PlainText.h"

namespace {

uint16_t seconds(JsonVariantConst value) {
    long s = value | 0L;
    return uint16_t(s < 0 ? 0 : s > 65535 ? 65535 : s);
}

}  // namespace

bool parseNowPlaying(const char* json, NowPlaying& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc["title"].is<const char*>() || !doc["playing"].is<bool>()) {
        return false;
    }
    out = NowPlaying();
    toPlainText(doc["title"], out.title, sizeof(out.title));
    toPlainText(doc["artist"] | "", out.artist, sizeof(out.artist));
    toPlainText(doc["album"] | "", out.album, sizeof(out.album));
    out.playing = doc["playing"];
    out.position = seconds(doc["position"]);
    out.duration = seconds(doc["duration"]);
    if (out.duration > 0 && out.position > out.duration) {
        out.position = out.duration;
    }
    return true;
}
