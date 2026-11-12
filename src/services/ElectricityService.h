#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../app/AppData.h"

// Electricity spot prices for Finland from the free api.spot-hinta.fi
// (no key). Gets today's prices, and tomorrow's once they are published
// (in the afternoon), per 15 minutes, in c/kWh including VAT.
//
// The API allows one request a minute per IP (1440 a day); more gives
// HTTP 429 and may block the IP for a while. main.cpp asks far less often.
//
// Returns how many prices were written to `out` (sorted by time), or -1 on
// failure (`error` says why).
int fetchPrices(PricePoint* out, int capacity, String& error);

// The prices from an API answer that is already parsed (also used by tests).
// Returns how many were written, sorted by time.
int readPrices(JsonArrayConst list, PricePoint* out, int capacity);
