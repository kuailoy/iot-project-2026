#include "IcsParser.h"

#include <new>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../app/DateUtil.h"

using namespace DateUtil;

namespace {

constexpr int LINE_SIZE = 512;        // Longer lines are cut (only titles can be that long)
constexpr int MAX_RECURRING = IcsParser::MAX_RECURRING;
constexpr int MAX_OVERRIDES = IcsParser::MAX_OVERRIDES;
constexpr int MAX_EXDATES = 12;       // Excluded dates per recurring event
constexpr int MAX_ORDINAL_DAYS = 7;   // BYDAY entries like "2TU" or "-1FR"
constexpr uint32_t MAX_STEPS = 20000; // Safety limit when expanding one rule

// A point in local time
struct LocalTime {
    int32_t day = 0;
    int16_t min = 0;
    bool allDay = false;
    bool valid = false;
    bool utc = false;       // Given in UTC: recurring events then repeat in UTC
    int32_t utcDay = 0;
    int16_t utcMin = 0;
};

struct Rule {
    enum Freq : uint8_t { NONE, DAILY, WEEKLY, MONTHLY, YEARLY };
    Freq freq = NONE;
    uint16_t interval = 1;
    uint16_t count = 0;           // 0 = no limit
    LocalTime until;              // valid = has an end date
    uint8_t weekdayMask = 0;      // BYDAY without a number, bit 0 = Monday
    int8_t ordinal[MAX_ORDINAL_DAYS];
    int8_t ordinalWeekday[MAX_ORDINAL_DAYS];
    uint8_t ordinalCount = 0;
    int8_t byMonthDay = 0;        // 0 = not set, negative = from the end of the month
};

struct Event {
    char title[40];
    char location[24];
    uint32_t uid;
    LocalTime start;
    LocalTime end;
    int32_t durationMin;          // From DURATION, -1 if not given
    Rule rule;
    LocalTime exdates[MAX_EXDATES];
    uint8_t exdateCount;
    LocalTime recurrenceId;       // Set when this replaces one occurrence of a recurring event
    bool cancelled;
};

struct Override {
    uint32_t uid;
    int32_t day;
    int16_t min;
};

// ---------- Small parsing helpers ----------

uint32_t hashText(const char* text) {
    uint32_t hash = 2166136261u;  // FNV-1a
    for (; *text; text++) {
        hash = (hash ^ uint8_t(*text)) * 16777619u;
    }
    return hash;
}

bool readNumber(const char* text, int digits, int& out) {
    out = 0;
    for (int i = 0; i < digits; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        out = out * 10 + (text[i] - '0');
    }
    return true;
}

LocalTime utcToLocal(int32_t day, int min) {
    time_t t = time_t(int64_t(day) * 86400 + min * 60);
    struct tm local;
    localtime_r(&t, &local);
    LocalTime result;
    result.day = dayOf(local);
    result.min = minuteOf(local);
    result.valid = true;
    return result;
}

// "20261005" (all day), "20261005T090000" (local or TZID) or "20261005T070000Z" (UTC)
LocalTime parseTime(const char* params, const char* value) {
    LocalTime result;
    int year, month, day;
    if (!readNumber(value, 4, year) || !readNumber(value + 4, 2, month) || !readNumber(value + 6, 2, day)) {
        return result;
    }

    bool dateOnly = value[8] != 'T' || (params != nullptr && strstr(params, "VALUE=DATE") != nullptr &&
                                         strstr(params, "VALUE=DATE-TIME") == nullptr);
    int32_t days = daysFromCivil(year, month, day);
    if (dateOnly) {
        result.day = days;
        result.allDay = true;
        result.valid = true;
        return result;
    }

    int hour, minute;
    if (!readNumber(value + 9, 2, hour) || !readNumber(value + 11, 2, minute)) {
        return result;
    }
    if (value[15] == 'Z') {
        result = utcToLocal(days, hour * 60 + minute);
        result.utc = true;
        result.utcDay = days;
        result.utcMin = hour * 60 + minute;
        return result;
    }
    result.day = days;
    result.min = hour * 60 + minute;
    result.valid = true;
    return result;
}

// "PT1H30M", "P1D", "P2W" -> minutes
int32_t parseDuration(const char* value) {
    int32_t total = 0;
    int32_t number = 0;
    bool negative = false;
    for (const char* c = value; *c; c++) {
        if (*c == '-') {
            negative = true;
        } else if (*c >= '0' && *c <= '9') {
            number = number * 10 + (*c - '0');
        } else {
            switch (*c) {
                case 'W': total += number * 7 * 1440; break;
                case 'D': total += number * 1440; break;
                case 'H': total += number * 60; break;
                case 'M': total += number; break;
                default: break;  // P, T, S
            }
            number = 0;
        }
    }
    return negative ? -total : total;
}

int weekdayCode(const char* code) {
    static const char* const CODES[7] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
    for (int i = 0; i < 7; i++) {
        if (strncmp(code, CODES[i], 2) == 0) {
            return i;
        }
    }
    return -1;
}

void parseRule(char* value, Rule& rule) {
    char* savePart;
    for (char* part = strtok_r(value, ";", &savePart); part; part = strtok_r(nullptr, ";", &savePart)) {
        char* eq = strchr(part, '=');
        if (eq == nullptr) {
            continue;
        }
        *eq = '\0';
        const char* key = part;
        char* val = eq + 1;

        if (strcmp(key, "FREQ") == 0) {
            rule.freq = strcmp(val, "DAILY") == 0     ? Rule::DAILY
                      : strcmp(val, "WEEKLY") == 0    ? Rule::WEEKLY
                      : strcmp(val, "MONTHLY") == 0   ? Rule::MONTHLY
                      : strcmp(val, "YEARLY") == 0    ? Rule::YEARLY
                                                      : Rule::NONE;  // HOURLY etc: shown once
        } else if (strcmp(key, "INTERVAL") == 0) {
            int n = atoi(val);
            rule.interval = n > 0 ? n : 1;
        } else if (strcmp(key, "COUNT") == 0) {
            rule.count = atoi(val);
        } else if (strcmp(key, "UNTIL") == 0) {
            rule.until = parseTime(nullptr, val);
        } else if (strcmp(key, "BYMONTHDAY") == 0) {
            rule.byMonthDay = atoi(val);  // Only the first value is used
        } else if (strcmp(key, "BYDAY") == 0) {
            char* saveDay;
            for (char* d = strtok_r(val, ",", &saveDay); d; d = strtok_r(nullptr, ",", &saveDay)) {
                int ordinal = atoi(d);  // "2TU" -> 2, "-1FR" -> -1, "MO" -> 0
                const char* code = d;
                while (*code == '+' || *code == '-' || (*code >= '0' && *code <= '9')) {
                    code++;
                }
                int wd = weekdayCode(code);
                if (wd < 0) {
                    continue;
                }
                if (ordinal == 0) {
                    rule.weekdayMask |= 1 << wd;
                } else if (rule.ordinalCount < MAX_ORDINAL_DAYS) {
                    rule.ordinal[rule.ordinalCount] = ordinal;
                    rule.ordinalWeekday[rule.ordinalCount] = wd;
                    rule.ordinalCount++;
                }
            }
        }
    }
}

// Copy an iCal text value: undo escapes, and turn UTF-8 into the plain
// ASCII the display fonts have (a umlaut -> a, dashes -> -, emoji dropped).
void copyText(char* out, size_t size, const char* in) {
    // U+00C0 .. U+00FF
    static const char LATIN1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    size_t n = 0;
    const uint8_t* p = (const uint8_t*)in;

    auto put = [&](char c) {
        if (n + 1 >= size) {
            return;
        }
        if (c == ' ' && (n == 0 || out[n - 1] == ' ')) {
            return;  // No leading or double spaces
        }
        out[n++] = c;
    };

    while (*p) {
        uint8_t c = *p;
        if (c == '\\' && p[1]) {
            char e = char(p[1]);
            put(e == 'n' || e == 'N' ? ' ' : e);
            p += 2;
        } else if (c < 0x80) {
            put(c < 0x20 ? ' ' : char(c));
            p++;
        } else if ((c & 0xE0) == 0xC0 && p[1]) {
            uint16_t code = uint16_t((c & 0x1F) << 6 | (p[1] & 0x3F));
            if (code >= 0xC0 && code <= 0xFF) {
                put(LATIN1[code - 0xC0]);
            } else if (code == 0xA0) {
                put(' ');
            } else {
                put('?');
            }
            p += 2;
        } else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
            uint16_t code = uint16_t((c & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F));
            if (code == 0x2013 || code == 0x2014) {
                put('-');
            } else if (code == 0x2018 || code == 0x2019) {
                put('\'');
            } else if (code == 0x201C || code == 0x201D) {
                put('"');
            }
            p += 3;  // Other symbols are dropped
        } else {
            // Emoji (4-byte) or a broken sequence: skip it
            p++;
            while ((*p & 0xC0) == 0x80) {
                p++;
            }
        }
    }
    while (n > 0 && out[n - 1] == ' ') {
        n--;
    }
    out[n] = '\0';
}

// Timetable titles (Tuudo) look like "IoT Project 1 IT24SP (IT00BD93-3002)/ IoT Project 1, ...".
// Cut them before a course code in brackets so the name fits on screen.
// Brackets without a digit stay, e.g. "Lunch (with team)".
void shortenTitle(char* title) {
    char* open = strstr(title, " (");
    if (open == nullptr || open - title < 4) {
        return;
    }
    for (const char* c = open + 2; *c && *c != ')'; c++) {
        if (*c >= '0' && *c <= '9') {
            *open = '\0';
            return;
        }
    }
}

// Keep locations short: "Microkatu 1, 70210 Kuopio, Finland" -> "Microkatu 1".
// Tuudo writes "Useita" (Finnish for "several") instead of the rooms and
// doesn't list them anywhere, so no location is shown then.
void shortenLocation(char* location) {
    if (strcmp(location, "Useita") == 0) {
        location[0] = '\0';
        return;
    }
    char* comma = strchr(location, ',');
    if (comma != nullptr && comma != location) {
        *comma = '\0';
    }
}

// ---------- Parser ----------

// Keeps only what is needed for the shown days
class Parser : public IcsParser {
public:
    Parser(int32_t firstDay, int32_t lastDay, CalendarEvent* out, int capacity)
        : firstDay(firstDay), lastDay(lastDay), out(out), capacity(capacity), unsureFrom(lastDay + 1) {}

    void feed(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            feedByte(data[i]);
        }
    }

    void finish() override {
        endLine();
        for (int i = 0; i < recurringCount; i++) {
            expand(recurring[i]);
        }
    }

    bool sawCalendar() const override { return calendarFound; }
    int count() const override { return outCount; }
    int dropped() const override { return droppedSeries + droppedOverrides + droppedEvents; }

    int32_t completeUntil() const override {
        int32_t until = unsureFrom - 1;
        // When the list is full the latest events are dropped,
        // so the last kept day may be incomplete
        if (droppedEvents > 0 && outCount > 0 && out[outCount - 1].day - 1 < until) {
            until = out[outCount - 1].day - 1;
        }
        return until;
    }

private:
    const int32_t firstDay;
    const int32_t lastDay;
    CalendarEvent* const out;
    const int capacity;
    int outCount = 0;
    int droppedSeries = 0;     // Recurring events over MAX_RECURRING
    int droppedOverrides = 0;  // Moved or cancelled occurrences over MAX_OVERRIDES
    int droppedEvents = 0;     // Occurrences over capacity
    int32_t unsureFrom;        // First day a left-out series or change can affect
    uint8_t bomChecked = 0;

    char line[LINE_SIZE];
    int length = 0;
    bool afterNewline = false;

    bool calendarFound = false;
    bool inEvent = false;
    int nested = 0;  // VALARM etc. inside an event
    Event current;

    Event recurring[MAX_RECURRING];
    int recurringCount = 0;
    Override overrides[MAX_OVERRIDES];
    int overrideCount = 0;

    void feedByte(uint8_t c) {
        // Skip a UTF-8 byte order mark at the start (some Outlook exports)
        if (bomChecked < 3) {
            static const uint8_t BOM[3] = {0xEF, 0xBB, 0xBF};
            if (c == BOM[bomChecked]) {
                bomChecked++;
                return;
            }
            bomChecked = 3;
        }
        if (c == '\r') {
            return;
        }
        if (afterNewline) {
            afterNewline = false;
            if (c == ' ' || c == '\t') {
                return;  // Folded line: continues the previous one
            }
            endLine();
        }
        if (c == '\n') {
            afterNewline = true;
        } else if (length < LINE_SIZE - 1) {
            line[length++] = char(c);
        }
    }

    // Something was left out that changes this day or later ones
    void markUnsure(int32_t day) {
        if (day < firstDay) {
            day = firstDay;
        }
        if (day < unsureFrom) {
            unsureFrom = day;
        }
    }

    void endLine() {
        if (length > 0) {
            line[length] = '\0';
            handleLine();
        }
        length = 0;
    }

    void handleLine() {
        // NAME;PARAM=...;PARAM="...":VALUE  (a ':' inside quotes is not the separator)
        char* value = nullptr;
        bool quoted = false;
        for (char* c = line; *c; c++) {
            if (*c == '"') {
                quoted = !quoted;
            } else if (*c == ':' && !quoted) {
                *c = '\0';
                value = c + 1;
                break;
            }
        }
        if (value == nullptr) {
            return;
        }
        char* params = strchr(line, ';');
        if (params != nullptr) {
            *params++ = '\0';
        }
        const char* name = line;

        if (strcmp(name, "BEGIN") == 0) {
            if (strcmp(value, "VCALENDAR") == 0) {
                calendarFound = true;
            } else if (strcmp(value, "VEVENT") == 0 && !inEvent) {
                inEvent = true;
                nested = 0;
                current = Event();  // All zero, then the defaults above
                current.durationMin = -1;
            } else if (inEvent) {
                nested++;
            }
            return;
        }
        if (strcmp(name, "END") == 0) {
            if (inEvent && nested > 0) {
                nested--;
            } else if (inEvent && strcmp(value, "VEVENT") == 0) {
                inEvent = false;
                finishEvent();
            }
            return;
        }
        if (!inEvent || nested > 0) {
            return;
        }

        if (strcmp(name, "SUMMARY") == 0) {
            // Shorten before cutting to size, so the course code is still found
            char title[96];
            copyText(title, sizeof(title), value);
            shortenTitle(title);
            strlcpy(current.title, title, sizeof(current.title));
        } else if (strcmp(name, "LOCATION") == 0) {
            char location[96];
            copyText(location, sizeof(location), value);
            shortenLocation(location);
            strlcpy(current.location, location, sizeof(current.location));
        } else if (strcmp(name, "UID") == 0) {
            current.uid = hashText(value);
        } else if (strcmp(name, "DTSTART") == 0) {
            current.start = parseTime(params, value);
        } else if (strcmp(name, "DTEND") == 0) {
            current.end = parseTime(params, value);
        } else if (strcmp(name, "DURATION") == 0) {
            current.durationMin = parseDuration(value);
        } else if (strcmp(name, "RRULE") == 0) {
            parseRule(value, current.rule);
        } else if (strcmp(name, "RECURRENCE-ID") == 0) {
            current.recurrenceId = parseTime(params, value);
        } else if (strcmp(name, "STATUS") == 0) {
            current.cancelled = strcmp(value, "CANCELLED") == 0;
        } else if (strcmp(name, "EXDATE") == 0) {
            char* save;
            for (char* d = strtok_r(value, ",", &save); d; d = strtok_r(nullptr, ",", &save)) {
                if (current.exdateCount < MAX_EXDATES) {
                    current.exdates[current.exdateCount++] = parseTime(params, d);
                }
            }
        }
    }

    void finishEvent() {
        Event& e = current;
        if (!e.start.valid) {
            return;
        }
        if (e.title[0] == '\0') {
            strcpy(e.title, "(No title)");
        }

        // Length of the event in minutes (all-day: whole days)
        if (e.durationMin < 0) {
            if (e.end.valid) {
                e.durationMin = (e.end.day - e.start.day) * 1440 + (e.end.min - e.start.min);
            } else {
                e.durationMin = e.start.allDay ? 1440 : 0;
            }
        }

        // A moved or cancelled occurrence: hide the original one. Without
        // room for it, the original would show too, so that day is unknown.
        if (e.recurrenceId.valid) {
            if (overrideCount < MAX_OVERRIDES) {
                overrides[overrideCount++] = {e.uid, e.recurrenceId.day, e.recurrenceId.min};
            } else {
                droppedOverrides++;
                if (e.recurrenceId.day >= firstDay && e.recurrenceId.day <= lastDay) {
                    markUnsure(e.recurrenceId.day);
                }
            }
        }
        if (e.cancelled) {
            return;
        }

        if (e.rule.freq != Rule::NONE && !e.recurrenceId.valid) {
            bool reachesWindow = e.start.day <= lastDay && (!e.rule.until.valid || e.rule.until.day >= firstDay - 31);
            if (!reachesWindow) {
                return;
            }
            if (recurringCount < MAX_RECURRING) {
                recurring[recurringCount++] = e;
            } else {
                droppedSeries++;  // Its occurrences are missing from its first day on
                markUnsure(e.start.day);
            }
            return;
        }

        addOccurrence(e, e.start.day, e.start.min);
    }

    // ---------- Recurring events ----------

    bool excluded(const Event& e, int32_t day, int16_t min) const {
        for (int i = 0; i < e.exdateCount; i++) {
            const LocalTime& ex = e.exdates[i];
            if (ex.valid && ex.day == day && (e.start.allDay || ex.allDay || ex.min == min)) {
                return true;
            }
        }
        for (int i = 0; i < overrideCount; i++) {
            const Override& o = overrides[i];
            if (o.uid == e.uid && o.day == day && (e.start.allDay || o.min == min)) {
                return true;
            }
        }
        return false;
    }

    void expand(const Event& e) {
        const Rule& r = e.rule;
        uint32_t emitted = 0;
        bool done = false;

        // Dates repeat in the time zone DTSTART was given in: local, or UTC
        // (then each occurrence is converted, so DST changes are handled)
        const int32_t startDay = e.start.utc ? e.start.utcDay : e.start.day;

        // Without COUNT, dates before the shown days don't matter: jump close
        // to them instead of stepping through years of old occurrences.
        // Keep a margin for multi-day events and the UTC/local date difference.
        const int32_t skipTo = firstDay - (e.durationMin / 1440 + 2);
        const bool canSkip = r.count == 0 && skipTo > startDay;

        // One candidate date; returns false once the rule has ended
        auto emit = [&](int32_t day) -> bool {
            if (day < startDay) {
                return true;  // Before DTSTART: not part of the series
            }
            LocalTime at;
            if (e.start.utc) {
                at = utcToLocal(day, e.start.utcMin);
            } else {
                at.day = day;
                at.min = e.start.min;
            }
            if (r.until.valid && (at.day > r.until.day ||
                                  (at.day == r.until.day && !e.start.allDay && !r.until.allDay &&
                                   at.min > r.until.min))) {
                return false;
            }
            if (r.count > 0 && emitted >= r.count) {
                return false;
            }
            emitted++;  // COUNT includes excluded dates (RFC 5545)
            if (at.day > lastDay) {
                return false;
            }
            if (!excluded(e, at.day, at.min)) {
                addOccurrence(e, at.day, at.min);
            }
            return true;
        };

        if (r.freq == Rule::DAILY) {
            uint32_t steps = 0;
            int32_t first = startDay;
            if (canSkip) {
                first += (skipTo - startDay) / r.interval * r.interval;
            }
            for (int32_t day = first; !done && steps < MAX_STEPS; day += r.interval, steps++) {
                if (r.weekdayMask && !(r.weekdayMask & (1 << weekday(day)))) {
                    continue;
                }
                done = !emit(day);
            }
        } else if (r.freq == Rule::WEEKLY) {
            uint8_t mask = r.weekdayMask ? r.weekdayMask : uint8_t(1 << weekday(startDay));
            int32_t monday = startDay - weekday(startDay);
            if (canSkip) {
                monday += (skipTo - monday) / (7 * r.interval) * 7 * r.interval;
            }
            for (uint32_t steps = 0; !done && steps < MAX_STEPS; steps++, monday += 7 * r.interval) {
                for (int wd = 0; wd < 7 && !done; wd++) {
                    if (mask & (1 << wd)) {
                        done = !emit(monday + wd);
                    }
                }
            }
        } else {
            // MONTHLY, and YEARLY as every 12 months in the month of DTSTART
            int monthStep = r.freq == Rule::YEARLY ? 12 * r.interval : r.interval;
            int year, month, dayOfMonth;
            civilFromDays(startDay, year, month, dayOfMonth);
            if (canSkip) {
                int skipYear, skipMonth, skipDay;
                civilFromDays(skipTo, skipYear, skipMonth, skipDay);
                int behind = (skipYear * 12 + skipMonth) - (year * 12 + month);
                int total = year * 12 + (month - 1) + behind / monthStep * monthStep;
                year = total / 12;
                month = total % 12 + 1;
            }

            for (uint32_t steps = 0; !done && steps < MAX_STEPS; steps++) {
                int length = daysInMonth(year, month);
                int32_t first = daysFromCivil(year, month, 1);
                if (first > lastDay) {
                    break;
                }

                if (r.weekdayMask || r.ordinalCount > 0) {
                    for (int d = 1; d <= length && !done; d++) {
                        int32_t day = first + d - 1;
                        int wd = weekday(day);
                        bool match = r.weekdayMask & (1 << wd);
                        for (int i = 0; i < r.ordinalCount && !match; i++) {
                            if (r.ordinalWeekday[i] != wd) {
                                continue;
                            }
                            int fromStart = (d - 1) / 7 + 1;
                            int fromEnd = (length - d) / 7 + 1;
                            match = r.ordinal[i] > 0 ? fromStart == r.ordinal[i] : fromEnd == -r.ordinal[i];
                        }
                        if (match) {
                            done = !emit(day);
                        }
                    }
                } else {
                    int d = r.byMonthDay > 0 ? r.byMonthDay
                          : r.byMonthDay < 0 ? length + 1 + r.byMonthDay
                                             : dayOfMonth;
                    if (d >= 1 && d <= length) {  // e.g. no 31st in April: skipped
                        done = !emit(first + d - 1);
                    }
                }

                month += monthStep;
                while (month > 12) {
                    month -= 12;
                    year++;
                }
            }
        }
    }

    // ---------- Output ----------

    // Adds one occurrence; all-day events over several days get one entry per day
    void addOccurrence(const Event& e, int32_t day, int16_t startMin) {
        if (e.start.allDay) {
            int32_t days = e.durationMin >= 1440 ? e.durationMin / 1440 : 1;
            for (int32_t d = day; d < day + days; d++) {
                if (d >= firstDay && d <= lastDay) {
                    CalendarEvent ev;
                    ev.day = d;
                    ev.startMin = 0;
                    ev.endMin = 1440;
                    ev.allDay = true;
                    strlcpy(ev.title, e.title, sizeof(ev.title));
                    strlcpy(ev.location, e.location, sizeof(ev.location));
                    insert(ev);
                }
            }
            return;
        }

        if (day < firstDay || day > lastDay) {
            return;
        }
        int32_t end = startMin + (e.durationMin > 0 ? e.durationMin : 0);
        CalendarEvent ev;
        ev.day = day;
        ev.startMin = startMin;
        ev.endMin = int16_t(end > 1440 ? 1440 : end);  // Past midnight: shown on the start day only
        ev.allDay = false;
        strlcpy(ev.title, e.title, sizeof(ev.title));
        strlcpy(ev.location, e.location, sizeof(ev.location));
        insert(ev);
    }

    static bool before(const CalendarEvent& a, const CalendarEvent& b) {
        if (a.day != b.day) {
            return a.day < b.day;
        }
        if (a.allDay != b.allDay) {
            return a.allDay;  // All-day events first
        }
        return a.startMin < b.startMin;
    }

    // Sorted insert; when full, the latest event is dropped
    void insert(const CalendarEvent& ev) {
        if (outCount == capacity) {
            if (!before(ev, out[capacity - 1])) {
                droppedEvents++;
                return;
            }
            outCount--;
            droppedEvents++;
        }
        int i = outCount;
        while (i > 0 && before(ev, out[i - 1])) {
            out[i] = out[i - 1];
            i--;
        }
        out[i] = ev;
        outCount++;
    }
};

}  // namespace

IcsParser* IcsParser::create(int32_t firstDay, int32_t lastDay, CalendarEvent* events, int capacity) {
    return new (std::nothrow) Parser(firstDay, lastDay, events, capacity);
}
