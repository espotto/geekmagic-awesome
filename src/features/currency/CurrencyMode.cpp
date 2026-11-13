#include "CurrencyMode.h"
#include <Arduino_GFX_Library.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "CurrencyJson.h"
#include "Gfx.h"
#include "Net.h"
#include "../theme/Languages.h"

CurrencyMode g_currencyMode;

static const uint32_t kEveryMs = 30UL * 60 * 1000;
static const uint32_t kRetryMs = 2UL * 60 * 1000;

bool CurrencyMode::fetch(const Settings& s) {
  char codes[currencyview::kMax][4];
  const int n = currencyview::parseCodes(s.currency.codes.c_str(), codes);
  if (!n) { error_ = F("no currencies chosen"); return false; }
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(4000);
  http.useHTTP10(true);   // no chunked transfer: the body streams straight into the parser
  if (!http.begin(client, F("http://www.cbr-xml-daily.ru/daily_json.js"))) { error_ = F("bad request"); return false; }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    error_ = code < 0 ? HTTPClient::errorToString(code) : String(F("HTTP ")) + code;
    http.end();
    return false;
  }
  JsonDocument filter;
  currencyview::buildFilter(filter, codes, n);
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  currencyview::Data d;
  if (err || !currencyview::fromJson(doc.as<JsonObjectConst>(), codes, n, d)) {
    error_ = err ? String(err.c_str()) : String(F("no data"));
    return false;
  }
  data_ = d;
  error_ = "";
  return true;
}

void CurrencyMode::service(const Settings& s) {
  if (codes_ != s.currency.codes) { codes_ = s.currency.codes; dueAt_ = 0; data_ = currencyview::Data(); failed_ = false; version_++; }
  if (netConnected() && (!dueAt_ || (int32_t)(millis() - dueAt_) >= 0)) {
    if (fetch(s)) {
      fetchedAt_ = millis();
      dueAt_ = millis() + kEveryMs;
      failed_ = false;
    } else {
      dueAt_ = millis() + kRetryMs;
      failed_ = true;
    }
    if (!dueAt_) dueAt_ = 1;
    version_++;
  }
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = version_ * 4 + lang + 1;
  if (key == drawn_) return;
  drawn_ = key;
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  currencyview::Layout l;
  currencyview::layout(data_, failed_ ? currencyview::Empty::Failed : currencyview::Empty::Loading, lang, l);
  uint16_t row[240];
  for (int y = 0; y < 240; y++) {
    currencyview::renderRow(l, y, row);
    for (int x = 0; x < 240; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, 240, 1);
    if ((y & 15) == 15) yield();
  }
}

void CurrencyMode::toJson(JsonObject out) const {
  out["valid"] = data_.valid;
  if (data_.valid) {
    char date[12];
    snprintf(date, sizeof(date), "%02u.%02u", (unsigned)data_.day, (unsigned)data_.month);
    out["date"] = date;
    JsonArray rs = out["rates"].to<JsonArray>();
    for (int i = 0; i < data_.count; ++i) {
      const currencyview::Rate& r = data_.rates[i];
      JsonObject o = rs.add<JsonObject>();
      o["code"] = r.code;
      if (r.valid) {
        o["value"] = r.value;
        o["prev"] = r.prev;
      }
    }
    out["age"] = (millis() - fetchedAt_) / 1000;
  }
  if (error_.length()) out["error"] = error_;
}
