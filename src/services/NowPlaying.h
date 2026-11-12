#pragma once

#include <stdint.h>

// What YouTube Music plays in the user's browser, as sent to
// http://desk.local/music by the extension in tools/desk-extension:
//   {"title": "Fourth of July", "artist": "Sufjan Stevens", "album": "Carrie & Lowell",
//    "playing": true, "position": 102, "duration": 218}
// Every change is sent, and every 15 s while the tab is open (see MusicData.h).
// A message with an empty title means the tab was closed.
// The extension already turns titles into plain ASCII (the display fonts have
// nothing else); any other byte that is left becomes '?'.
struct NowPlaying {
    char title[64] = "";       // "" = the YouTube Music tab was closed
    char artist[48] = "";
    char album[48] = "";
    bool playing = false;      // false = paused
    uint16_t position = 0;     // Seconds into the track
    uint16_t duration = 0;     // Seconds, 0 = unknown (e.g. a live stream)
};

// false if the text is not in the format above
bool parseNowPlaying(const char* json, NowPlaying& out);
