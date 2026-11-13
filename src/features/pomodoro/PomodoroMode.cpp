#include "PomodoroMode.h"
#include <Arduino_GFX_Library.h>
#include "Clock.h"
#include "Gfx.h"
#include "PomodoroView.h"
#include "../theme/Languages.h"
#include "NotifyMode.h"

PomodoroMode g_pomodoroMode;

pomo::Plan PomodoroMode::plan(const Settings& s) {
  pomo::Plan p;
  p.focusMin = s.pomodoro.focusMin;
  p.shortMin = s.pomodoro.shortMin;
  p.longMin = s.pomodoro.longMin;
  p.rounds = s.pomodoro.rounds;
  p.autoNext = s.pomodoro.autoNext;
  return p;
}

void PomodoroMode::tick(const Settings& s) {
  pomo::Phase ended;
  if (!core_.tick(millis(), plan(s), ended)) return;
  if (core_.run() != pomo::Run::Running) overlay_ = false;   // the next phase waits: give the screen back
  drawnKey_ = 0;
  char title[32], text[48];
  pomoview::bannerWords(gm::languageFromCode(s.clock.lang.c_str()), ended, title, sizeof(title), text, sizeof(text));
  NotifyRequest r;
  r.type = "done";
  r.title = title;
  r.label = text;
  r.color = ended == pomo::Phase::Focus ? "green" : "red";
  r.ttlSec = 20;
  r.prio = 2;   // over any other notice
  g_notifyMode.request(r);
}

int PomodoroMode::apply(JsonVariantConst body, const Settings& s, String& error) {
  if (!body.is<JsonObjectConst>()) { error = F("expected a JSON object"); return 400; }
  const pomo::Plan p = plan(s);
  const uint32_t now = millis();
  bool any = false;
  if (body["reset"] | false) { any = true; core_.reset(); overlay_ = false; }
  if (body["skip"] | false) { any = true; core_.skip(now, p); }
  if (body["pause"] | false) { any = true; core_.pause(now); }
  if (body["start"] | false) {
    any = true;
    core_.start(now, p);
    overlay_ = body["show"] | true;
  } else if (!body["show"].isNull()) {
    any = true;
    overlay_ = body["show"] | false;
  }
  if (body["hide"] | false) { any = true; overlay_ = false; }
  if (!any) { error = F("nothing to do: start, pause, skip, reset, show or hide"); return 400; }
  drawnKey_ = 0;
  return 200;
}

void PomodoroMode::toJson(JsonObject out, const Settings& s) const {
  const pomo::Plan p = plan(s);
  const uint32_t now = millis();
  out["state"] = core_.run() == pomo::Run::Running ? "running" : core_.run() == pomo::Run::Paused ? "paused" : "idle";
  out["phase"] = core_.phase() == pomo::Phase::Focus ? "focus" : core_.phase() == pomo::Phase::Short ? "short" : "long";
  out["left"] = (core_.leftMs(now, p) + 999) / 1000;
  out["total"] = core_.totalMs(p) / 1000;
  out["done"] = core_.done();
  out["rounds"] = p.rounds;
  out["show"] = overlay_;
}

void PomodoroMode::service(const Settings& s) {
  const pomo::Plan p = plan(s);
  const uint32_t left = core_.leftMs(millis(), p);
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = ((left + 999) / 1000) * 1024 + (uint32_t)core_.run() * 256 + (uint32_t)core_.phase() * 64 +
                       core_.done() * 4 + lang + 1;
  if (key == drawnKey_) return;
  drawnKey_ = key;
  int endH = -1, endM = 0;
  struct tm t;
  if (core_.run() == pomo::Run::Running && clockNow(t)) {
    const uint32_t at = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec + (left + 999) / 1000;
    endH = (at / 3600) % 24;
    endM = (at / 60) % 60;
  }
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  timerview::Layout l;
  pomoview::layout(core_, p, millis(), endH, endM, lang, l);
  uint16_t row[240];
  for (int y = 0; y < 240; y++) {   // every row: the app before it may have filled the edges
    timerview::renderRow(l, y, row);
    for (int x = 0; x < 240; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, 240, 1);
    if ((y & 15) == 15) yield();
  }
}
