// CurrencyMode.h — the Exchange Rates app: the Bank of Russia's official rates
// for up to four currencies (settings "currency"), with the change since the
// previous day, from www.cbr-xml-daily.ru over plain HTTP, no key. Fetched
// when the app is on the screen and the data is older than 30 minutes (2
// minutes after a failure; the Bank sets the rates once a working day);
// drawn by CurrencyView. /api/currency serves the same data to the web panel.
#pragma once
#include "config.h"
#include <ArduinoJson.h>
#include "Mode.h"
#include "CurrencyView.h"

class CurrencyMode : public DisplayMode {
 public:
  const char* id() const override { return "currency"; }
  uint8_t modeConst() const override { return MODE_CURRENCY; }
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawn_ = 0; dueAt_ = 0; }
  void wake(const Settings&) override { drawn_ = 0; }

  void toJson(JsonObject out) const;
  void refresh() { dueAt_ = 0; }

 private:
  bool fetch(const Settings& s);
  currencyview::Data data_;
  String codes_;             // the list the data is for: a changed one fetches again
  uint32_t fetchedAt_ = 0;
  uint32_t dueAt_ = 0;       // 0: fetch as soon as possible
  uint32_t version_ = 1, drawn_ = 0;
  bool failed_ = false;
  String error_;
};

extern CurrencyMode g_currencyMode;
