const DEFAULTS = { org: "", device: "desk.local", minutes: 5 };
const $ = (id) => document.getElementById(id);

async function load() {
  const settings = await chrome.storage.sync.get(DEFAULTS);
  $("org").value = settings.org;
  $("device").value = settings.device;
  $("minutes").value = settings.minutes;
  showStatus();
}

async function showStatus() {
  const { status, statusAt } = await chrome.storage.local.get(["status", "statusAt"]);
  $("status").textContent = status ? `${status} (${new Date(statusAt).toLocaleTimeString()})` : "Nothing sent yet";
}

$("save").onclick = async () => {
  await chrome.storage.sync.set({
    org: $("org").value.trim(),
    device: $("device").value.trim() || DEFAULTS.device,
    minutes: Math.max(1, Number($("minutes").value) || DEFAULTS.minutes),
  });
  $("status").textContent = "Saved, sending...";  // Saving restarts the timer and sends once
};

$("send").onclick = () => {
  $("status").textContent = "Sending...";
  chrome.runtime.sendMessage("sync");
};

chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "local") {
    showStatus();
  }
});

load();
