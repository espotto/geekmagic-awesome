// CalendarMode.h — the Calendar app: this month as a grid, today circled
// (CalendarView). It draws once and again only when the date or the language
// changes, so it costs nothing between midnights. Needs the clock: until NTP
// sets it, a short message says so.
#pragma once
#include "config.h"
#include "Mode.h"

class CalendarMode : public DisplayMode {
 public:
  const char* id() const override { return "calendar"; }
  uint8_t modeConst() const override { return MODE_CALENDAR; }
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawn_ = 0; waiting_ = false; }
 private:
  uint32_t drawn_ = 0;     // the date and language on screen (0: nothing)
  bool waiting_ = false;   // the waiting-for-the-clock message is up
};

extern CalendarMode g_calendarMode;
