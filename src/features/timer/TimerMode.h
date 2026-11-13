// TimerMode.h — the Timer app: one countdown, started from the web panel or
// any HTTP client (/api/timer, e.g. an iPhone Shortcut). Starting it also puts
// it on the screen over the current app until it ends or is cancelled ("show":
// false keeps it in the background); its own app in the list shows it any
// time. When it reaches zero a banner says so (NotifyMode) and the screen goes
// back to what it showed. Counted with millis(): no clock needed, though the
// screen shows when it will end once the clock is set.
#pragma once
#include "config.h"
#include <ArduinoJson.h>
#include "Mode.h"

class TimerMode : public DisplayMode {
 public:
  const char* id() const override { return "timer"; }
  uint8_t modeConst() const override { return MODE_TIMER; }
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawnKey_ = 0; }

  // Every loop pass, on screen or not: notices the end, raises the banner.
  void tick(const Settings& s);
  // Over the current app while it runs (started with "show").
  bool overlay() const { return overlay_ && running(); }
  bool running() const { return state_ != 0; }

  void toJson(JsonObject out) const;
  // "start" (seconds, 1-86399) with optional "label" and "show"; "pause";
  // "resume"; "cancel"; "add" (seconds, to a running or paused timer); "hide".
  int apply(JsonVariantConst body, String& error);

 private:
  uint32_t leftMs() const;
  uint8_t state_ = 0;          // 0 idle, 1 running, 2 paused
  uint32_t totalMs_ = 300000;  // the last duration set (idle shows it)
  uint32_t endAt_ = 0;         // millis() at zero, while running
  uint32_t pausedLeft_ = 0;    // left when paused
  bool overlay_ = false;
  char label_[24] = "";
  uint32_t drawnKey_ = 0;
};

extern TimerMode g_timerMode;
