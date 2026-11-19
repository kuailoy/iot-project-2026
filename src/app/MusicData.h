#pragma once

#include <stdio.h>

#include "AppData.h"

// Read-only helpers for "now playing" in AppData (Music page and widget).
// The extension sends every 15 s while the tab is open; without a message
// for MUSIC_STALE_S the browser was closed, and nothing counts as playing.
constexpr long MUSIC_STALE_S = 60;

namespace MusicData {

// A song is open in YouTube Music (playing or paused) and the extension still sends
inline bool active(const AppData& data) {
    return data.musicValid && data.musicOpen && data.musicTitle[0] != '\0' &&
           data.epoch - data.musicUpdated <= MUSIC_STALE_S;
}

// A song was seen before: shown as "last played" while nothing is active
inline bool hasLast(const AppData& data) {
    return data.musicValid && data.musicTitle[0] != '\0';
}

// Seconds into the song now: counted on from the last message while playing
inline int position(const AppData& data) {
    long seconds = data.musicPosition;
    if (data.musicPlaying && data.epoch > data.musicUpdated) {
        seconds += long(data.epoch - data.musicUpdated);
    }
    if (data.musicDuration > 0 && seconds > data.musicDuration) {
        seconds = data.musicDuration;
    }
    return int(seconds);
}

// "1:42", "1:02:03"
inline void formatTime(char* text, size_t size, int seconds) {
    if (seconds >= 3600) {
        snprintf(text, size, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    } else {
        snprintf(text, size, "%d:%02d", seconds / 60, seconds % 60);
    }
}

}
