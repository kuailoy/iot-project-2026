#include "CalendarService.h"
#include "IcalParser.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// HTTPClient writes the body into a Stream, this one hands it to the parser
class ParserStream : public Stream {
public:
    explicit ParserStream(IcalParser& parser) : parser(parser) {}

    size_t write(uint8_t c) override {
        parser.feed((char)c);
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        parser.feed((const char*)buffer, size);
        return size;
    }

    // Nothing to read back
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

private:
    IcalParser& parser;
};

bool fetchCalendar(const char* url, int32_t fromDay, int32_t toDay, CalendarData& out,
                   String& error) {
    if (url == nullptr || url[0] == '\0') {
        error = "CALENDAR_URL is not set in .env.local";
        return false;
    }

    String link = url;
    if (link.startsWith("webcal://")) {
        link = "https://" + link.substring(9);
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, link)) {
        error = "Could not initialize the calendar request";
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        error = status > 0 ? String("HTTP ") + status : http.errorToString(status);
        http.end();
        return false;
    }

    // Parse into out while downloading, then mark it valid only if nothing went wrong
    IcalParser parser(out, fromDay, toDay);
    ParserStream stream(parser);
    int written = http.writeToStream(&stream);
    http.end();
    parser.finish();

    if (written < 0) {
        error = "Download stopped: " + http.errorToString(written);
        return false;
    }
    out.valid = true;
    error = "";
    return true;
}
