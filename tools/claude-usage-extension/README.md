# Claude usage extension

Sends your Claude plan usage (Pro/Max: 5-hour session and weekly limit) to the desk display, for the **Claude** widget on the second home page.

Claude has no official API for these numbers. This extension reads them the same way the page
[claude.ai/settings/usage](https://claude.ai/settings/usage) does, in your own browser where you are logged in.
Your claude.ai cookies stay in the browser; only four numbers go to the display:

```json
{"session": 54, "session_resets": "2026-11-04T17:30:00+00:00", "weekly": 29, "weekly_resets": "2026-11-06T20:00:00+00:00"}
```

## Install (Chrome, Edge, Brave)

1. Open `chrome://extensions`, turn on **Developer mode**.
2. **Load unpacked** → choose this folder (`tools/claude-usage-extension`).
3. Click the extension's icon and fill in:
   - **Organization ID**: open claude.ai/settings/usage, DevTools → Network, find the request
     `/api/organizations/<ID>/usage` and copy the ID.
   - **Display address**: `desk.local` (or the display's IP from Menu → Settings).
4. **Save**. It sends right away and then every 5 minutes while the browser is open.

The display shows "x min ago" on the widget when no numbers came for 30 minutes
(for example, the browser was closed).

## When it stops working

The claude.ai address is unofficial and may change. The display doesn't need to change:
fix `extract()` in `background.js` so it still sends the same four values.
The status line in the extension's window says what went wrong.
