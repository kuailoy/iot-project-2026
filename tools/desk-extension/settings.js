// Settings shared by background.js and the extension's window (options.html).
// The display address is used until another one is saved in the window.

const DEFAULTS = { device: "10.214.33.156", org: "", minutes: 5 };

// An empty saved address means the default one
async function getSettings() {
  const settings = await chrome.storage.sync.get(DEFAULTS);
  settings.device = settings.device || DEFAULTS.device;
  return settings;
}

// What was sent last, or why it wasn't, per feature ("claude", "music")
async function setStatus(kind, text) {
  await chrome.storage.local.set({ [kind]: { text, at: Date.now() } });
}
