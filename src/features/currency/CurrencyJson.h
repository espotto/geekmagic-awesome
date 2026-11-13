// CurrencyJson.h — reads the Bank of Russia's daily rates as served by
// www.cbr-xml-daily.ru/daily_json.js into currencyview::Data, for the codes
// asked. Header-only, shared by the device and the tests.
//
//   {"Date":"2026-11-14T11:30:00+03:00", ...,
//    "Valute":{"USD":{"Nominal":1,"Value":82.1516,"Previous":82.9803,...},...}}
//
// Value and Previous are roubles for Nominal units (100 yen, 10000 sum).
#pragma once
#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>
#include "CurrencyView.h"

namespace currencyview {

// Only the fields the screen uses, for a streaming parse that stays small.
inline void buildFilter(JsonDocument& filter, const char codes[kMax][4], int n) {
  filter["Date"] = true;
  for (int i = 0; i < n; ++i) {
    JsonObject f = filter["Valute"][codes[i]].to<JsonObject>();
    f["Nominal"] = true;
    f["Value"] = true;
    f["Previous"] = true;
  }
}

// False when the answer has no date or none of the codes.
inline bool fromJson(JsonObjectConst root, const char codes[kMax][4], int n, Data& out) {
  out = Data();
  const char* date = root["Date"] | "";   // "2026-11-14T..."
  if (strlen(date) >= 10) {
    out.month = (uint8_t)atoi(date + 5);
    out.day = (uint8_t)atoi(date + 8);
  }
  bool any = false;
  for (int i = 0; i < n && i < kMax; ++i) {
    Rate& r = out.rates[out.count++];
    memcpy(r.code, codes[i], 4);
    JsonObjectConst v = root["Valute"][codes[i]];
    const float nominal = v["Nominal"] | 0.0f;
    if (v.isNull() || nominal <= 0) continue;
    r.value = (v["Value"] | 0.0f) / nominal;
    r.prev = (v["Previous"] | 0.0f) / nominal;
    r.valid = r.value > 0;
    any = any || r.valid;
  }
  out.valid = any && out.day && out.month;
  return out.valid;
}

}  // namespace currencyview
