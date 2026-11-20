#include "PlainText.h"

#include <stdint.h>
#include <string.h>

namespace {

// U+00C0 .. U+00FF without their accents (ß, Æ, æ are spelled out below)
const char LATIN1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

// U+0100 .. U+017F (Latin Extended-A: Polish, Czech, Baltic, Turkish...) without accents
const char LATIN_A[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
static_assert(sizeof(LATIN_A) - 1 == 0x80, "one letter for every code point U+0100..U+017F");

// U+0430 .. U+044F, Cyrillic lowercase (Ukrainian spelling, like the extension)
const char* const CYRILLIC[32] = {"a",  "b", "v",  "h",  "d",  "e",    "zh", "z", "y", "i",  "k",
                                  "l",  "m", "n",  "o",  "p",  "r",    "s",  "t", "u", "f",  "kh",
                                  "ts", "ch", "sh", "shch", "", "y",   "",   "e", "iu", "ia"};

// Spelled-out letters and punctuation
struct Special {
    uint16_t code;
    const char* text;
};
const Special SPECIAL[] = {
    {0x00A0, " "},   {0x00C6, "AE"},  {0x00DF, "ss"}, {0x00E6, "ae"}, {0x0132, "IJ"}, {0x0133, "ij"},
    {0x0152, "OE"},  {0x0153, "oe"},  {0x0401, "Io"}, {0x0404, "Ie"}, {0x0406, "I"},  {0x0407, "I"},
    {0x0451, "io"},  {0x0454, "ie"},  {0x0456, "i"},  {0x0457, "i"},  {0x0490, "G"},  {0x0491, "g"},
    {0x2010, "-"},   {0x2011, "-"},   {0x2012, "-"},  {0x2013, "-"},  {0x2014, "-"},  {0x2018, "'"},
    {0x2019, "'"},   {0x201A, "'"},   {0x201C, "\""}, {0x201D, "\""}, {0x201E, "\""}, {0x2022, "-"},
    {0x2026, "..."}, {0x2032, "'"},   {0x2033, "\""},
};

// The plain text for code point `code`; "" to leave it out, "?" for an unknown letter
const char* plainFor(uint32_t code, char* buffer) {
    for (const Special& s : SPECIAL) {
        if (s.code == code) {
            return s.text;
        }
    }
    buffer[1] = '\0';
    if (code >= 0xC0 && code <= 0xFF) {
        buffer[0] = LATIN1[code - 0xC0];
        return buffer;
    }
    if (code >= 0x100 && code <= 0x17F) {
        buffer[0] = LATIN_A[code - 0x100];
        return buffer;
    }
    // Cyrillic: А..Я and а..я; the first letter stays a capital ("Zh")
    bool upper = code >= 0x410 && code <= 0x42F;
    if (upper || (code >= 0x430 && code <= 0x44F)) {
        const char* plain = CYRILLIC[code - (upper ? 0x410 : 0x430)];
        strncpy(buffer, plain, 7);
        buffer[7] = '\0';
        if (upper && buffer[0] >= 'a' && buffer[0] <= 'z') {
            buffer[0] = char(buffer[0] - 'a' + 'A');
        }
        return buffer;
    }
    if (code >= 0x300 && code <= 0x36F) {
        return "";  // An accent written after its letter ("e" + U+0301): the letter is enough
    }
    if (code >= 0x2000 && code <= 0x200A) {
        return " ";  // Thin, wide and other spaces
    }
    if (code < 0x2000 || (code >= 0x3000 && code < 0xE000) || code >= 0xF900) {
        // Letters of other scripts (Greek, Hebrew, CJK...) can't be shown: one "?"
        return code >= 0x1F000 ? "" : "?";
    }
    return "";  // Symbols, arrows, private use: left out
}

}  // namespace

void toPlainText(const char* utf8, char* out, size_t outSize) {
    if (outSize == 0) {
        return;
    }
    size_t n = 0;
    auto put = [&](const char* text) {
        for (; *text && n + 1 < outSize; text++) {
            if (*text == ' ' && (n == 0 || out[n - 1] == ' ')) {
                continue;  // No leading or double spaces
            }
            out[n++] = *text;
        }
    };

    const uint8_t* p = (const uint8_t*)(utf8 != nullptr ? utf8 : "");
    char buffer[8];
    while (*p) {
        uint8_t c = *p;
        if (c < 0x80) {
            char plain[2] = {c < 0x20 ? ' ' : char(c), '\0'};
            put(plain);
            p++;
            continue;
        }
        // Length of the UTF-8 sequence, and its bits
        int length = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        uint32_t code = length == 2 ? (c & 0x1F) : length == 3 ? (c & 0x0F) : (c & 0x07);
        bool valid = length > 1;
        for (int i = 1; i < length && valid; i++) {
            valid = (p[i] & 0xC0) == 0x80;
            code = code << 6 | (p[i] & 0x3F);
        }
        if (!valid) {
            p++;  // A broken byte: skip it
            continue;
        }
        put(plainFor(code, buffer));
        p += length;
    }
    while (n > 0 && out[n - 1] == ' ') {
        n--;
    }
    out[n] = '\0';
}
