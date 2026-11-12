#include "NowPlaying.h"

#include <ArduinoJson.h>
#include <string.h>

namespace {

// Copy as printable ASCII: the display fonts have no other characters
void copyAscii(char* out, size_t size, const char* in) {
    size_t n = 0;
    for (; in != nullptr && *in && n + 1 < size; in++) {
        unsigned char c = (unsigned char)*in;
        if (c >= 0x80 && (c & 0xC0) == 0x80) {
            continue;  // Rest of a UTF-8 character: one '?' is enough
        }
        out[n++] = (c >= 0x20 && c < 0x7F) ? char(c) : '?';
    }
    out[n] = '\0';
}

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
    copyAscii(out.title, sizeof(out.title), doc["title"]);
    copyAscii(out.artist, sizeof(out.artist), doc["artist"] | "");
    copyAscii(out.album, sizeof(out.album), doc["album"] | "");
    out.playing = doc["playing"];
    out.position = seconds(doc["position"]);
    out.duration = seconds(doc["duration"]);
    if (out.duration > 0 && out.position > out.duration) {
        out.position = out.duration;
    }
    return true;
}
