// Reads the Claude plan usage from claude.ai in this browser (where you are
// logged in) and sends only the numbers to the desk display:
//   {"session": 54, "session_resets": "...", "weekly": 29, "weekly_resets": "..."}
// Your claude.ai cookies never leave the browser.
//
// The claude.ai address is not an official API: if claude.ai changes it,
// only extract() below needs fixing, not the display.

const DEFAULTS = { org: "", device: "desk.local", minutes: 5 };

async function getSettings() {
  return { ...DEFAULTS, ...(await chrome.storage.sync.get(DEFAULTS)) };
}

async function setStatus(text) {
  await chrome.storage.local.set({ status: text, statusAt: Date.now() });
}

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

async function sync() {
  const settings = await getSettings();
  if (!settings.org) {
    await setStatus("Add your organization ID first");
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
    await setStatus(`Sent: session ${Math.round(numbers.session)}%, week ${Math.round(numbers.weekly)}%`);
  } catch (error) {
    const reason = error instanceof TypeError ? `can't reach ${settings.device} or claude.ai` : error.message;
    await setStatus(`Not sent: ${reason}`);
  }
}

async function schedule() {
  const settings = await getSettings();
  const minutes = Math.max(1, Number(settings.minutes) || DEFAULTS.minutes);
  await chrome.alarms.create("sync", { periodInMinutes: minutes });
  await sync();
}

chrome.runtime.onInstalled.addListener(schedule);
chrome.runtime.onStartup.addListener(schedule);
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === "sync") {
    sync();
  }
});
chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "sync") {
    schedule();
  }
});
chrome.runtime.onMessage.addListener((message) => {
  if (message === "sync") {
    sync();
  }
});
