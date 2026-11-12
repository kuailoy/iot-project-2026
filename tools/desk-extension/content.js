// Runs inside music.youtube.com: reads the song and tells background.js,
// which sends it to the display. Sends on every change (song, play/pause)
// and every 15 s, so the display knows the tab is still open.

const HEARTBEAT_MS = 15000;
let lastKey = "";
let lastSent = 0;

function text(selector) {
  const element = document.querySelector(selector);
  return element ? element.textContent.trim() : "";
}

function read() {
  const video = document.querySelector("video");
  const meta = navigator.mediaSession && navigator.mediaSession.metadata;
  // The page's own "now playing" info, or else the player bar at the bottom
  const title = (meta && meta.title) || text("ytmusic-player-bar .title");
  if (!video || !title) {
    return { title: "", playing: false };  // Nothing loaded
  }
  const byline = text("ytmusic-player-bar .byline").split(" • ");  // "Artist • Album • 2015"
  return {
    title,
    artist: (meta && meta.artist) || byline[0] || "",
    album: (meta && meta.album) || "",
    playing: !video.paused && !video.ended,
    position: Math.floor(video.currentTime || 0),
    duration: isFinite(video.duration) ? Math.floor(video.duration) : 0,
  };
}

function send(song) {
  try {
    chrome.runtime.sendMessage({ type: "song", song });
  } catch (error) {
    // The extension was reloaded: this old copy of the script can't send any more
  }
}

function check() {
  const song = read();
  const key = JSON.stringify([song.title, song.artist, song.album, song.playing]);
  if (key !== lastKey || Date.now() - lastSent >= HEARTBEAT_MS) {
    lastKey = key;
    lastSent = Date.now();
    send(song);
  }
}

setInterval(check, 2000);
// Media events don't bubble, but they can be caught on the way down
for (const name of ["play", "pause", "seeked", "ended"]) {
  document.addEventListener(name, check, true);
}
window.addEventListener("pagehide", () => send({ title: "", playing: false }));
