#include "CalendarService.h"
#include "IcsParser.h"
#include "../app/DateUtil.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <memory>

namespace {

// HTTPClient writes the download into a Stream, this one hands it to the parser
class ParserStream : public Stream {
public:
    explicit ParserStream(IcsParser& parser) : parser(parser) {}

    size_t write(uint8_t c) override {
        parser.feed(&c, 1);
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        parser.feed(buffer, size);
        return size;
    }

    // Nothing to read back
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

private:
    IcsParser& parser;
};

}  // namespace

int fetchCalendar(const char* url, int32_t firstDay, int32_t lastDay,
                  CalendarEvent* events, int capacity, int32_t& completeUntil, String& error) {
    if (url == nullptr || url[0] == '\0') {
        error = "No calendar link: add it on the settings page (desk.local)";
        return -1;
    }

    // webcal:// links are plain HTTPS
    String address = url;
    if (address.startsWith("webcal://")) {
        address = "https://" + address.substring(9);
    }

    // TODO: Verify the server certificate (the weather request skips it too)
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, address)) {
        error = "Could not start the calendar request";
        return -1;
    }

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        error = status > 0 ? String("HTTP ") + status : HTTPClient::errorToString(status);
        http.end();
        return -1;
    }

    std::unique_ptr<IcsParser> parser(IcsParser::create(firstDay, lastDay, events, capacity));
    if (!parser) {
        error = "Not enough RAM to read the calendar";
        http.end();
        return -1;
    }

    unsigned long start = millis();
    ParserStream stream(*parser);
    int bytes = http.writeToStream(&stream);
    http.end();
    if (bytes < 0) {
        error = "Download failed: " + HTTPClient::errorToString(bytes);
        return -1;
    }

    parser->finish();
    if (!parser->sawCalendar()) {
        error = "The address did not return an iCal calendar";
        return -1;
    }

    Serial.printf("Calendar: %d bytes in %lu ms, %d events", bytes, millis() - start, parser->count());
    if (parser->dropped() > 0) {
        int year, month, day;
        DateUtil::civilFromDays(parser->completeUntil() + 1, year, month, day);
        Serial.printf(" (%d left out: too many, unknown from %04d-%02d-%02d on)", parser->dropped(), year, month,
                      day);
    }
    Serial.println();
    completeUntil = parser->completeUntil();
    return parser->count();
}
