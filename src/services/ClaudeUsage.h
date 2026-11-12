#pragma once

#include <time.h>

// Claude plan usage (claude.ai Pro/Max), as sent to http://desk.local/claude
// by the browser extension in tools/claude-usage-extension:
//   {"session": 54, "session_resets": "2026-11-04T17:30:00.29+00:00",
//    "weekly": 29, "weekly_resets": "2026-11-06T20:00:00.29+00:00"}
// There is no official API for these numbers; the extension reads the
// claude.ai usage page in the user's own logged-in browser.
struct ClaudeUsage {
    float sessionPercent = 0;   // Used of the 5-hour limit
    time_t sessionResets = 0;   // UTC, 0 = unknown
    float weeklyPercent = 0;    // Used of the weekly limit
    time_t weeklyResets = 0;
};

// false if the text is not in the format above
bool parseClaudeUsage(const char* json, ClaudeUsage& out);
