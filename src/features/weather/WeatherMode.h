// WeatherMode.h — the Weather app: the current weather at the device's
// location (Display tab -> Location) from Open-Meteo, over plain HTTP, no key.
// Fetched when the app is on the screen, or a watch face that shows the
// weather is, and the data is older than 15 minutes (2 minutes after a
// failure); drawn by WeatherView. /api/weather serves the same data to the web
// panel, and a watch face reads it as device.weather* (ThemeMode).
#pragma once
#include "config.h"
#include <ArduinoJson.h>
#include "Mode.h"
#include "WeatherView.h"

class WeatherMode : public DisplayMode {
 public:
  const char* id() const override { return "weather"; }
  uint8_t modeConst() const override { return MODE_WEATHER; }
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawn_ = 0; dueAt_ = 0; }
  void wake(const Settings&) override { drawn_ = 0; }

  void toJson(JsonObject out) const;
  void refresh() { dueAt_ = 0; tried_ = 0; }
  // Fetches when due, without drawing: for a watch face that shows the weather.
  void keepFresh(const Settings& s);
  // The last reading, and a number that changes whenever it does.
  const weatherview::Data& data() const { return data_; }
  uint32_t version() const { return version_; }

 private:
  bool fetch(const Settings& s);
  int fetchFrom(const Settings& s, const char* host);   // HTTP status, <0 no connection, 0 bad body
  weatherview::Data data_;
  uint32_t fetchedAt_ = 0;   // millis() of the last good fetch
  uint32_t tried_ = 0;       // millis() of the last attempt (0: none)
  uint32_t dueAt_ = 0;       // 0: fetch as soon as possible
  uint32_t version_ = 1, drawn_ = 0;
  String error_;
};

extern WeatherMode g_weatherMode;
