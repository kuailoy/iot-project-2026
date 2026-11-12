const $ = (id) => document.getElementById(id);

// An empty address field means the default in settings.js
async function load() {
  const { device, org, minutes } = await chrome.storage.sync.get(["device", "org", "minutes"]);
  $("device").placeholder = DEFAULTS.device;
  $("device").value = device || "";
  $("org").value = org || "";
  $("minutes").placeholder = DEFAULTS.minutes;
  $("minutes").value = minutes || DEFAULTS.minutes;
  showStatus();
}

async function showStatus() {
  const statuses = await chrome.storage.local.get(["claude", "music"]);
  for (const kind of ["claude", "music"]) {
    const status = statuses[kind];
    $(`${kind}-status`).textContent = status
      ? `${status.text} (${new Date(status.at).toLocaleTimeString()})`
      : "Nothing sent yet";
  }
}

// One write, so background.js restarts the Claude timer (and sends) once
$("save").onclick = async () => {
  await chrome.storage.sync.set({
    device: $("device").value.trim(),
    org: $("org").value.trim(),
    minutes: Math.max(1, Number($("minutes").value) || DEFAULTS.minutes),
  });
  $("saved").textContent = "Saved";
};

$("claude").onclick = () => {
  $("claude-status").textContent = "Sending...";
  chrome.runtime.sendMessage("claude");
};

$("music").onclick = () => {
  $("music-status").textContent = "Sending...";
  chrome.runtime.sendMessage("music-test");
};

chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "local") {
    showStatus();
  }
});

load();
