#pragma once

#include <Adafruit_GFX.h>

#include "../../app/AppData.h"

// Now playing in YouTube Music (sent by the browser extension, see NowPlaying.h),
// or "nothing playing" with the last song. Shows only, no playback controls yet.
// Designs: assets/UI-figma-E-ink/21_eink-now-playing-spotify-1280x800 (16_10).png,
//          22_eink-now-playing-empty-1280x800 1.png
class MusicPage {
public:
    void draw(Adafruit_GFX& gfx, const AppData& data);
};
