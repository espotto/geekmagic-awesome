#include "WeatherMode.h"
#include <Arduino_GFX_Library.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "Gfx.h"
#include "Net.h"
#include "../theme/Languages.h"

WeatherMode g_weatherMode;

static const uint32_t kEveryMs = 15UL * 60 * 1000;
static const uint32_t kRetryMs = 2UL * 60 * 1000;

static bool hasLocation(const Settings& s) { return s.clock.lat != 0 || s.clock.lon != 0; }

// Open-Meteo answers the same forecast on two hosts. Some networks cannot
// reach the main one (it sits in an address range ISPs block), while the
// historical-forecast host, on other addresses, gets through; a failed
// connection moves to the other host, and the one that worked is kept.
static const char* const kHosts[] = {"api.open-meteo.com", "historical-forecast-api.open-meteo.com"};
static uint8_t g_host = 0;

bool WeatherMode::fetch(const Settings& s) {
  for (int attempt = 0; attempt < 2; attempt++) {
    const int code = fetchFrom(s, kHosts[g_host]);
    if (code > 0) return code == HTTP_CODE_OK;
    g_host ^= 1;   // could not connect: the other host next
  }
  return false;
}

int WeatherMode::fetchFrom(const Settings& s, const char* host) {
  char url[400];
  snprintf_P(url, sizeof(url),
             PSTR("http://%s/v1/forecast?latitude=%.4f&longitude=%.4f"
                  "&current=temperature_2m,weather_code,is_day,apparent_temperature,relative_humidity_2m,wind_speed_10m"
                  "&hourly=temperature_2m,weather_code,is_day&forecast_hours=%d"
                  "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto"),
             host, s.clock.lat, s.clock.lon, weatherview::kHours);
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(4000);
  http.useHTTP10(true);   // no chunked transfer: the body streams straight into the parser
  if (!http.begin(client, url)) { error_ = F("bad request"); return 0; }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    error_ = code < 0 ? HTTPClient::errorToString(code) : String(F("HTTP ")) + code;
    http.end();
    return code;
  }
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["apparent_temperature"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["hourly"]["time"] = true;
  filter["hourly"]["temperature_2m"] = true;
  filter["hourly"]["weather_code"] = true;
  filter["hourly"]["is_day"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err || doc["current"]["temperature_2m"].isNull()) {
    error_ = err ? String(err.c_str()) : String(F("no data"));
    return 0;
  }
  weatherview::Data d;
  d.valid = true;
  d.temp = doc["current"]["temperature_2m"] | 0.0f;
  d.code = doc["current"]["weather_code"] | 0;
  d.day = (doc["current"]["is_day"] | 1) != 0;
  d.feels = doc["current"]["apparent_temperature"] | d.temp;
  d.humidity = doc["current"]["relative_humidity_2m"] | 0;
  d.wind = doc["current"]["wind_speed_10m"] | 0.0f;
  d.high = doc["daily"]["temperature_2m_max"][0] | d.temp;
  d.low = doc["daily"]["temperature_2m_min"][0] | d.temp;
  for (int i = 0; i < weatherview::kHours; i++) {
    const char* t = doc["hourly"]["time"][i] | "";   // "2026-11-13T14:00"
    const char* tp = strchr(t, 'T');
    d.hours[i].hour = tp ? (uint8_t)atoi(tp + 1) : 0;
    const float v = doc["hourly"]["temperature_2m"][i] | 0.0f;
    d.hours[i].temp = (int8_t)(v < 0 ? v - 0.5f : v + 0.5f);
    d.hours[i].code = doc["hourly"]["weather_code"][i] | 0;
    d.hours[i].day = (doc["hourly"]["is_day"][i] | 1) != 0;
  }
  d.hours[0].temp = (int8_t)(d.temp < 0 ? d.temp - 0.5f : d.temp + 0.5f);   // "Now" is the current reading
  d.hours[0].code = d.code;
  d.hours[0].day = d.day;
  data_ = d;
  error_ = "";
  return HTTP_CODE_OK;
}

void WeatherMode::keepFresh(const Settings& s) {
  if (!hasLocation(s) || !netConnected() || (dueAt_ && (int32_t)(millis() - dueAt_) < 0)) return;
  tried_ = millis();
  if (fetch(s)) {
    fetchedAt_ = millis();
    dueAt_ = millis() + kEveryMs;
  } else {
    dueAt_ = millis() + kRetryMs;
  }
  if (!dueAt_) dueAt_ = 1;
  version_++;
}

void WeatherMode::service(const Settings& s) {
  keepFresh(s);
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = version_ * 16 + lang * 2 + (hasLocation(s) ? 1 : 0);
  if (key == drawn_) return;
  drawn_ = key;
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  weatherview::Layout l;
  weatherview::layout(data_, hasLocation(s) ? weatherview::Empty::Loading : weatherview::Empty::NoLocation, lang, l);
  uint16_t row[240];
  for (int y = 0; y < 240; y++) {
    weatherview::renderRow(l, y, row);
    for (int x = 0; x < 240; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, 240, 1);
    if ((y & 15) == 15) yield();
  }
}

void WeatherMode::toJson(JsonObject out) const {
  out["valid"] = data_.valid;
  if (data_.valid) {
    out["temp"] = data_.temp;
    out["code"] = data_.code;
    out["day"] = data_.day;
    out["high"] = data_.high;
    out["low"] = data_.low;
    out["feels"] = data_.feels;
    out["humidity"] = data_.humidity;
    out["wind"] = data_.wind;
    out["cond"] = weatherview::conditionFor(data_.code);
    JsonArray hs = out["hours"].to<JsonArray>();
    for (const auto& h : data_.hours) {
      JsonObject o = hs.add<JsonObject>();
      o["hour"] = h.hour;
      o["temp"] = h.temp;
      o["code"] = h.code;
      o["day"] = h.day;
    }
    out["age"] = (millis() - fetchedAt_) / 1000;
    out["host"] = kHosts[g_host];
  }
  if (error_.length()) out["error"] = error_;
}
