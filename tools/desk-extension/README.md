# Desk display extension

One Chrome extension that sends two things from your browser to the desk display:

- **Claude usage** (Pro/Max: 5-hour session and weekly limit), for the **Claude** widget.
- **Now playing**: what YouTube Music plays, for the **Music** page (menu → Music) and the **Now playing** widget.

## Install (Chrome, Edge, Brave)

1. Open `chrome://extensions`, turn on **Developer mode**.
2. **Load unpacked** → choose this folder (`tools/desk-extension`).
3. Click the extension's icon and fill in:
   - **Display address**: `desk.local`, or the display's IP (Menu → Settings on the display).
     Empty means the default in `settings.js`.
   - **Organization ID** (for Claude usage): open claude.ai/settings/usage, DevTools → Network,
     find the request `/api/organizations/<ID>/usage` and copy the ID. Leave it empty to not send Claude usage.
4. **Save**. **Send now** sends the Claude usage, **Send a test** shows a test song on the display.

The computer and the display must be on the same Wi-Fi. If you had the two older extensions
(Claude usage, Now playing) loaded, remove them; this one replaces both.

## Claude usage

Claude has no official API for these numbers. The extension reads them the same way the page
[claude.ai/settings/usage](https://claude.ai/settings/usage) does, in your own browser where you are logged in.
Your claude.ai cookies stay in the browser; only four numbers go to the display (`POST /claude`):

```json
{"session": 54, "session_resets": "2026-11-04T17:30:00+00:00", "weekly": 29, "weekly_resets": "2026-11-06T20:00:00+00:00"}
```

It sends after **Save** and then every 5 minutes (the window can change this) while the browser is open.
The display shows "x min ago" on the widget when no numbers came for 30 minutes
(for example, the browser was closed).

## Now playing

The extension reads the song in the music.youtube.com tab and sends only these values to the display
(`POST /music`):

```json
{"title": "Fourth of July", "artist": "Sufjan Stevens", "album": "Carrie & Lowell", "playing": true, "position": 102, "duration": 218}
```

It sends when the song or play/pause changes, and every 15 seconds while the tab is open.
The display moves the progress bar on by itself and redraws at most once a minute (E-Ink);
the song, artist and play/pause show right away. Without a message for a minute (browser closed),
the display shows "Nothing playing" with the last song.

The display's fonts only have plain letters, so titles are converted first: accents are dropped
("Beyoncé" → "Beyonce"), Cyrillic is written in Latin letters ("Океан Ельзи" → "Okean Elzy"),
other scripts become "?" and emoji are left out.

Playback controls from the display (play/pause, next) are not supported yet.

## Files

- `settings.js`: the default settings, shared by the window and the background worker.
- `background.js`: reads claude.ai and sends both kinds of messages to the display.
- `content.js`: runs inside music.youtube.com and reads the song.
- `options.html`, `options.js`: the extension's window.

## When it stops working

The extension's window has a status line for each part: what was sent last, or why it wasn't.

- **Claude usage**: the claude.ai address is unofficial and may change. Fix `extract()` in `background.js`
  so it still sends the same four values; the display doesn't need to change.
- **Now playing**: the song is read from the page's media info (`navigator.mediaSession`), with the player bar
  at the bottom of the page as a fallback (`ytmusic-player-bar .title` and `.byline`). If YouTube Music changes
  its page, fix `read()` in `content.js`.
