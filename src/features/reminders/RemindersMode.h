// RemindersMode.h — the Reminders app: a short to-do list sent from a phone
// (the web panel or any HTTP client, /api/reminders) that stays on the screen
// until it is cleared. The list lives in flash (/reminders.json), so it
// survives a restart. The screen is drawn again only when the list or the
// language changes.
#pragma once
#include "config.h"
#include <ArduinoJson.h>
#include "Mode.h"
#include "RemindersList.h"

class RemindersMode : public DisplayMode {
 public:
  const char* id() const override { return "reminders"; }
  uint8_t modeConst() const override { return MODE_REMINDERS; }
  void begin(const Settings&) override;
  void service(const Settings& s) override;
  void invalidate(const Settings&) override { drawn_ = 0; }

  // The list as {"open":N,"items":[{"text":..,"done":..},...]}.
  void toJson(JsonObject out) const;
  // Applies one request: "add" (a string or an array of them), "done" /
  // "undone" / "remove" (an index, as GET lists them), "clearDone", "clear",
  // or "items" (replaces the whole list). Several may come together; they run
  // in that order. Returns an HTTP status and fills `error` on failure.
  int apply(JsonVariantConst body, String& error);

 private:
  void load();
  bool save();
  reminders::List list_;
  uint32_t version_ = 1;   // bumped on every change
  uint32_t drawn_ = 0;     // version and language on screen (0: nothing)
};

extern RemindersMode g_remindersMode;
