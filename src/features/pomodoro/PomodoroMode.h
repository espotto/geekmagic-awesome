// PomodoroMode.h — the Pomodoro app: focus sessions and breaks on a ring, run
// from the web panel or any HTTP client (/api/pomodoro). Like the Timer,
// starting it puts it over the current app ("show": false keeps it in the
// background) and its own app in the list shows it any time. At the end of
// each phase a banner says what comes next; with autoNext it starts by itself.
// The durations are settings (settings "pomodoro"); the run is not saved, so
// a restart stops it.
#pragma once
#include "config.h"
#include <ArduinoJson.h>
#include "Mode.h"
#include "PomodoroCore.h"

class PomodoroMode : public DisplayMode {
 public:
  const char* id() const override { return "pomodoro"; }
  uint8_t modeConst() const override { return MODE_POMODORO; }
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawnKey_ = 0; }

  // Every loop pass, on screen or not: notices the end of a phase.
  void tick(const Settings& s);
  bool overlay() const { return overlay_ && core_.run() == pomo::Run::Running; }

  void toJson(JsonObject out, const Settings& s) const;
  // "start" (or resume), "pause", "skip", "reset", "show"/"hide".
  int apply(JsonVariantConst body, const Settings& s, String& error);

  static pomo::Plan plan(const Settings& s);

 private:
  pomo::Core core_;
  bool overlay_ = false;
  uint32_t drawnKey_ = 0;
};

extern PomodoroMode g_pomodoroMode;
