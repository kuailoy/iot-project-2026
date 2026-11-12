// Sends two things from this browser to the desk display:
//  - Claude plan usage, read from claude.ai where you are logged in, every few minutes (POST /claude):
//      {"session": 54, "session_resets": "...", "weekly": 29, "weekly_resets": "..."}
//  - what YouTube Music plays (content.js tells us), on every change (POST /music):
//      {"title": "...", "artist": "...", "album": "...", "playing": true, "position": 102, "duration": 218}
// Only these values leave the browser, and only to the display. Your claude.ai
// cookies never leave the browser.

importScripts("settings.js");

// ---------- Claude usage ----------
// The claude.ai address is not an official API: if claude.ai changes it,
// only extract() below needs fixing, not the display.

// Session (5-hour) and weekly numbers from the claude.ai answer
function extract(usage) {
  const pick = (windowName, limitKind) => {
    const w = usage[windowName];
    if (w && typeof w.utilization === "number") {
      return { percent: w.utilization, resets: w.resets_at || "" };
    }
    const limit = (usage.limits || []).find((l) => l.kind === limitKind);
    if (limit && typeof limit.percent === "number") {
      return { percent: limit.percent, resets: limit.resets_at || "" };
    }
    return null;
  };
  const session = pick("five_hour", "session");
  const weekly = pick("seven_day", "weekly_all");
  if (!session || !weekly) {
    throw new Error("claude.ai answered in an unknown format");
  }
  return {
    session: session.percent,
    session_resets: session.resets,
    weekly: weekly.percent,
    weekly_resets: weekly.resets,
  };
}

async function sendClaude() {
  const settings = await getSettings();
  if (!settings.org) {
    await setStatus("claude", "Off: add your organization ID");
    return;
  }
  try {
    const answer = await fetch(
      `https://claude.ai/api/organizations/${encodeURIComponent(settings.org)}/usage`,
      { credentials: "include" }
    );
    if (answer.status === 401 || answer.status === 403) {
      throw new Error("not logged in to claude.ai in this browser");
    }
    if (!answer.ok) {
      throw new Error(`claude.ai answered ${answer.status}`);
    }
    const numbers = extract(await answer.json());

    const sent = await fetch(`http://${settings.device}/claude`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(numbers),
    });
    if (!sent.ok) {
      throw new Error(`the display answered ${sent.status}`);
    }
    await setStatus("claude", `Sent: session ${Math.round(numbers.session)}%, week ${Math.round(numbers.weekly)}%`);
  } catch (error) {
    const reason = error instanceof TypeError ? `can't reach ${settings.device} or claude.ai` : error.message;
    await setStatus("claude", `Not sent: ${reason}`);
  }
}

// Sends now and then every few minutes; nothing runs without an organization ID
async function scheduleClaude() {
  const settings = await getSettings();
  await chrome.alarms.clear("claude");
  if (settings.org) {
    const minutes = Math.max(1, Number(settings.minutes) || DEFAULTS.minutes);
    await chrome.alarms.create("claude", { periodInMinutes: minutes });
  }
  await sendClaude();
}

chrome.runtime.onInstalled.addListener(scheduleClaude);
chrome.runtime.onStartup.addListener(scheduleClaude);
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === "claude") {
    sendClaude();
  }
});
chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "sync") {
    scheduleClaude();  // Saved in the extension's window
  }
});

// ---------- Now playing ----------
// The display's fonts are plain ASCII, so titles are transliterated here.

const CYRILLIC = {
  а: "a", б: "b", в: "v", г: "h", ґ: "g", д: "d", е: "e", є: "ie", ж: "zh", з: "z", и: "y", і: "i",
  ї: "i", й: "i", к: "k", л: "l", м: "m", н: "n", о: "o", п: "p", р: "r", с: "s", т: "t", у: "u",
  ф: "f", х: "kh", ц: "ts", ч: "ch", ш: "sh", щ: "shch", ь: "", ю: "iu", я: "ia", ы: "y", э: "e",
  ё: "io", ъ: "",
};
const OTHER = {
  "ß": "ss", "æ": "ae", "ø": "o", "œ": "oe", "ł": "l", "đ": "d", "‘": "'", "’": "'",
  "“": '"', "”": '"', "–": "-", "—": "-", "…": "...", "•": "-", " ": " ",
};

// "Beyoncé" -> "Beyonce", "Океан Ельзи" -> "Okean Elzy"; letters with no
// plain form (Chinese, Greek...) become "?", emoji are dropped
function ascii(text) {
  let out = "";
  for (const ch of (text || "").normalize("NFC")) {
    const lower = ch.toLowerCase();
    if (lower in CYRILLIC) {
      const plain = CYRILLIC[lower];
      out += ch !== lower ? plain.charAt(0).toUpperCase() + plain.slice(1) : plain;
    } else if (lower in OTHER) {
      out += OTHER[lower];
    } else {
      const plain = ch.normalize("NFKD").replace(/[̀-ͯ]/g, "");
      if (/^[\x20-\x7e]+$/.test(plain)) {
        out += plain;
      } else if (/[\p{L}\p{N}]/u.test(ch)) {
        out += "?";
      }
    }
  }
  return out.replace(/\s+/g, " ").trim();
}

async function sendSong(song) {
  const { device } = await getSettings();
  const body = {
    title: ascii(song.title),
    artist: ascii(song.artist),
    album: ascii(song.album),
    playing: !!song.playing,
    position: Math.max(0, Math.round(song.position || 0)),
    duration: Math.max(0, Math.round(song.duration || 0)),
  };
  try {
    const answer = await fetch(`http://${device}/music`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!answer.ok) {
      await setStatus("music", `Not sent: the display answered ${answer.status}`);
    } else if (body.title) {
      await setStatus("music", `Sent: ${body.playing ? "" : "(paused) "}${body.artist ? body.artist + " - " : ""}${body.title}`);
    } else {
      await setStatus("music", "Sent: nothing playing");
    }
  } catch (error) {
    await setStatus("music", `Not sent: can't reach ${device}`);
  }
}

// The YouTube Music tab was closed: tell the display right away
chrome.tabs.onRemoved.addListener(async (tabId) => {
  const { tabId: musicTab } = await chrome.storage.session.get("tabId");
  if (tabId === musicTab) {
    await chrome.storage.session.remove("tabId");
    sendSong({ title: "", playing: false });
  }
});

// ---------- Messages from content.js and the extension's window ----------

chrome.runtime.onMessage.addListener((message, sender) => {
  if (message && message.type === "song") {
    if (sender.tab) {
      chrome.storage.session.set({ tabId: sender.tab.id });
    }
    sendSong(message.song);
  } else if (message === "claude") {
    sendClaude();
  } else if (message === "music-test") {
    // Shows on the display until the next real message (15 s)
    sendSong({ title: "Test from the browser", artist: "Desk display", playing: false });
  }
});
