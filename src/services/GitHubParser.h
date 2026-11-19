#pragma once

#include <ArduinoJson.h>

#include "../app/AppData.h"

// The contribution graph from a parsed GitHub GraphQL answer
// (viewer.contributionsCollection.contributionCalendar, see GitHubService.cpp).
// Days are placed by their date; more than GITHUB_MAX_DAYS keeps the newest.
// No Arduino or network code, so the UI preview can test it on a PC.
// False if the answer has no contribution calendar.
bool readContributions(JsonObjectConst answer, GitHubContributions& out);
