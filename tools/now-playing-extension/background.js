// Sends what YouTube Music plays (content.js tells us) to the desk display:
//   {"title": "...", "artist": "...", "album": "...", "playing": true, "position": 102, "duration": 218}
// Only these values leave the browser, and only to the display.
// The display's fonts are plain ASCII, so titles are transliterated here.
// Display address: "desk.local", or set another one in the service worker console:
//   chrome.storage.sync.set({ device: "192.168.1.23" })

const DEFAULTS = { device: "10.214.33.156" };

const CYRILLIC = {
  а: "a", б: "b", в: "v", г: "h", ґ: "g", д: "d", е: "e", є: "ie", ж: "zh", з: "z", и: "y", і: "i",
  ї: "i", й: "i", к: "k", л: "l", м: "m", н: "n", о: "o", п: "p", р: "r", с: "s", т: "t", у: "u",
  ф: "f", х: "kh", ц: "ts", ч: "ch", ш: "sh", щ: "shch", ь: "", ю: "iu", я: "ia", ы: "y", э: "e",
  ё: "io", ъ: "",
};
const OTHER = {
  "ß": "ss", "æ": "ae", "ø": "o", "œ": "oe", "ł": "l", "đ": "d", "‘": "'", "’": "'",
  "“": '"', "”": '"', "–": "-", "—": "-", "…": "...", "•": "-", " ": " ",
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

async function send(song) {
  const { device } = await chrome.storage.sync.get(DEFAULTS);
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
      console.warn(`Now playing not sent: the display answered ${answer.status}`);
    }
  } catch (error) {
    console.warn(`Now playing not sent: can't reach ${device}`);
  }
}

chrome.runtime.onMessage.addListener((message, sender) => {
  if (message && message.type === "song") {
    if (sender.tab) {
      chrome.storage.session.set({ tabId: sender.tab.id });
    }
    send(message.song);
  }
});

// The YouTube Music tab was closed: tell the display right away
chrome.tabs.onRemoved.addListener(async (tabId) => {
  const { tabId: musicTab } = await chrome.storage.session.get("tabId");
  if (tabId === musicTab) {
    await chrome.storage.session.remove("tabId");
    send({ title: "", playing: false });
  }
});
