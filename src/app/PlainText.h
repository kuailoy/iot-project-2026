#pragma once

#include <stddef.h>

// The display's fonts only have plain ASCII, so text from outside (song
// titles, ...) is turned into it first, like the browser extension does:
//   accents dropped              "Beyoncé", "Łódź"  -> "Beyonce", "Lodz"
//   a few letters spelled out    "Straße", "Œuvre"  -> "Strasse", "OEuvre"
//   Cyrillic in Latin letters    "Океан Ельзи"      -> "Okean Elzy"
//   typographic punctuation      "’ “ ” – — …"      -> "' \" \" - - ..."
//   other scripts (Greek, CJK)   one "?" a letter
//   emoji and other symbols      left out
// Spaces are squeezed and trimmed. `out` always ends with '\0'.
// No Arduino code here, so the UI preview can test it on a PC.
void toPlainText(const char* utf8, char* out, size_t outSize);
