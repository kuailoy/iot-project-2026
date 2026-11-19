#pragma once

#include <Arduino.h>

#include "../app/AppData.h"

// Contribution graph of the last year for the GitHub user who owns `token`
// (GraphQL API, api.github.com/graphql; parsed by GitHubParser.h). The API
// needs a token for every request; one without any permissions is enough
// for public contributions.
//
// Returns false on failure (`error` says why, `tokenRefused` is set when
// GitHub doesn't accept the token); `out` is only complete on success.
bool fetchContributions(const char* token, GitHubContributions& out, String& error, bool& tokenRefused);
