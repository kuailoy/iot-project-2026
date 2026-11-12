#include "IcalParser.h"

#include <string.h>
#include <time.h>

// ---------- Small helpers ----------

static bool readNumber(const char* text, int digits, int* number) {
    *number = 0;
    for (int i = 0; i < digits; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        *number = *number * 10 + (text[i] - '0');
    }
    return true;
}

// "20261005T090000", "20261005T070000Z" or "20261005" (all-day)
static bool parseDateTime(const char* value, int32_t* day, int* minute,
                          bool* allDay) {
    int year, month, dayOfMonth;
    if (!readNumber(value, 4, &year) || !readNumber(value + 4, 2, &month) ||
        !readNumber(value + 6, 2, &dayOfMonth) || month < 1 || month > 12) {
        return false;
    }

    *day = CalendarDate::fromCivil(year, month, dayOfMonth);
    *minute = 0;
    *allDay = value[8] != 'T';  // VALUE=DATE has no time part
    if (*allDay) {
        return true;
    }

    int hour, min;
    if (!readNumber(value + 9, 2, &hour) || !readNumber(value + 11, 2, &min)) {
        return false;
    }

    if (value[15] == 'Z') {
        // UTC: let the C library apply the device time zone (set by configTzTime)
        time_t utc = (time_t)*day * 86400 + hour * 3600 + min * 60;
        struct tm* local = localtime(&utc);
        if (local != nullptr) {
            *day = CalendarDate::fromTm(*local);
            hour = local->tm_hour;
            min = local->tm_min;
        }
    }
    *minute = hour * 60 + min;
    return true;
}

// Copies one UTF-8 char as plain ASCII, the E-Ink fonts only have ASCII.
// Returns how many bytes were read, *out = 0 when the char is dropped.
static int toAscii(const unsigned char* text, char* out) {
    if (text[0] < 0x80) {
        *out = (char)text[0];
        return 1;
    }

    // Finnish and other common Latin letters: U+00C0 to U+00FF
    static const char LATIN[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    if (text[0] == 0xC3 && (text[1] & 0xC0) == 0x80) {
        *out = LATIN[(text[1] & 0x3F)];
        return 2;
    }

    // Anything else (symbols, other scripts): skip the whole char
    *out = 0;
    int length = 1;
    while ((text[length] & 0xC0) == 0x80) {
        length++;
    }
    return length;
}

// "IoT Project 1 IT24SP (IT00BD93-3002)/ Group (Teacher)" becomes "IoT Project 1 IT24SP".
// Also undoes the iCal escapes \, \; \n and \\.
static void cleanTitle(const char* value, char* title, size_t size) {
    size_t length = 0;
    const unsigned char* text = (const unsigned char*)value;

    while (*text != 0 && length + 1 < size) {
        // Course code and teachers in brackets are too long for the screen
        if (text[0] == ' ' && text[1] == '(' && length > 0) {
            break;
        }

        char c;
        if (text[0] == '\\' && text[1] != 0) {
            c = (text[1] == 'n' || text[1] == 'N') ? ' ' : (char)text[1];
            text += 2;
        } else {
            text += toAscii(text, &c);
        }

        if (c != 0 && !(c == ' ' && length == 0)) {
            title[length++] = c;
        }
    }

    while (length > 0 && title[length - 1] == ' ') {
        length--;
    }
    title[length] = 0;
}

// All-day events come before the timed events of the same day
static int32_t sortKey(const CalendarEvent& event) {
    return event.day * 2000 + (event.allDay ? -1 : event.startMinute);
}

// ---------- Parser ----------

IcalParser::IcalParser(CalendarData& out, int32_t fromDay, int32_t toDay)
    : calendar(out), firstDay(fromDay), lastDay(toDay) {
    calendar = CalendarData();
}

void IcalParser::feed(const char* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        feed(data[i]);
    }
}

void IcalParser::feed(char c) {
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        lineEnded = true;
        return;
    }

    if (lineEnded) {
        lineEnded = false;
        if (c == ' ' || c == '\t') {
            return;  // Folded line: the text goes on after this space
        }
        handleLine();
        lineLength = 0;
    }

    if (lineLength < LINE_SIZE - 1) {
        line[lineLength++] = c;
    }
}

void IcalParser::finish() {
    if (lineLength > 0) {
        handleLine();
        lineLength = 0;
    }
    lineEnded = false;
}

void IcalParser::handleLine() {
    line[lineLength] = 0;

    // NAME;PARAM=x;PARAM=y:VALUE, a ':' inside quotes belongs to a parameter
    char* colon = nullptr;
    bool quoted = false;
    for (char* p = line; *p; p++) {
        if (*p == '"') {
            quoted = !quoted;
        } else if (*p == ':' && !quoted) {
            colon = p;
            break;
        }
    }
    if (colon == nullptr) {
        return;
    }
    *colon = 0;
    const char* value = colon + 1;

    // Parameters like TZID are not needed, times are read as local time
    char* semicolon = strchr(line, ';');
    if (semicolon != nullptr) {
        *semicolon = 0;
    }

    handleProperty(line, value);
}

void IcalParser::handleProperty(const char* name, const char* value) {
    if (strcmp(name, "BEGIN") == 0) {
        if (inEvent) {
            nestedDepth++;
        } else if (strcmp(value, "VEVENT") == 0) {
            inEvent = true;
            nestedDepth = 0;
            hasStart = hasEnd = cancelled = false;
            current = CalendarEvent();
        }
        return;
    }

    if (strcmp(name, "END") == 0) {
        if (inEvent && nestedDepth > 0) {
            nestedDepth--;
        } else if (inEvent && strcmp(value, "VEVENT") == 0) {
            finishEvent();
            inEvent = false;
        }
        return;
    }

    // Lines of alarms inside the event would overwrite its title
    if (!inEvent || nestedDepth > 0) {
        return;
    }

    if (strcmp(name, "SUMMARY") == 0) {
        cleanTitle(value, current.title, sizeof(current.title));
    } else if (strcmp(name, "DTSTART") == 0) {
        int minute;
        hasStart = parseDateTime(value, &current.day, &minute, &current.allDay);
        current.startMinute = minute;
    } else if (strcmp(name, "DTEND") == 0) {
        int minute;
        bool allDay;
        hasEnd = parseDateTime(value, &endDay, &minute, &allDay);
        current.endMinute = minute;
    } else if (strcmp(name, "STATUS") == 0) {
        cancelled = strcmp(value, "CANCELLED") == 0;
    }
}

void IcalParser::finishEvent() {
    seen++;
    if (!hasStart || cancelled) {
        return;
    }

    // Times are shown for the start day only
    if (current.allDay) {
        current.startMinute = 0;
        current.endMinute = 24 * 60;
    } else if (!hasEnd || endDay < current.day ||
               (endDay == current.day && current.endMinute < current.startMinute)) {
        current.endMinute = current.startMinute;
    } else if (endDay > current.day) {
        current.endMinute = 24 * 60;
    }

    if (current.title[0] == 0) {
        strcpy(current.title, "(No title)");
    }

    if (current.day >= firstDay && current.day < lastDay) {
        addEvent(current);
    }
}

void IcalParser::addEvent(const CalendarEvent& event) {
    // Insert sorted by day and start time, all-day events first
    int index = calendar.count;
    while (index > 0 && sortKey(calendar.events[index - 1]) > sortKey(event)) {
        index--;
    }

    // When full, the latest events are dropped
    if (index >= CalendarData::MAX_EVENTS) {
        return;
    }
    int last = calendar.count < CalendarData::MAX_EVENTS ? calendar.count : CalendarData::MAX_EVENTS - 1;
    for (int i = last; i > index; i--) {
        calendar.events[i] = calendar.events[i - 1];
    }
    calendar.events[index] = event;
    if (calendar.count < CalendarData::MAX_EVENTS) {
        calendar.count++;
    }
}
