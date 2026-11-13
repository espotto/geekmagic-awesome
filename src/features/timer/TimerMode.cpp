#include "TimerMode.h"
#include <Arduino_GFX_Library.h>
#include "Clock.h"
#include "Gfx.h"
#include "TimerView.h"
#include "../theme/Languages.h"
#include "NotifyMode.h"

TimerMode g_timerMode;

uint32_t TimerMode::leftMs() const {
  if (state_ == 2) return pausedLeft_;
  if (state_ != 1) return totalMs_;
  const int32_t left = (int32_t)(endAt_ - millis());
  return left > 0 ? (uint32_t)left : 0;
}

void TimerMode::tick(const Settings& s) {
  if (state_ != 1 || (int32_t)(millis() - endAt_) < 0) return;
  state_ = 0;
  overlay_ = false;
  drawnKey_ = 0;
  char title[32], text[32];
  timerview::bannerWords(gm::languageFromCode(s.clock.lang.c_str()), title, sizeof(title), text, sizeof(text));
  NotifyRequest r;
  r.type = "done";
  r.title = label_[0] ? label_ : title;
  r.label = text;
  r.color = "orange";
  r.ttlSec = 30;
  r.prio = 2;   // over any other notice
  g_notifyMode.request(r);
}

int TimerMode::apply(JsonVariantConst body, String& error) {
  if (!body.is<JsonObjectConst>()) { error = F("expected a JSON object"); return 400; }
  bool any = false;
  if (!body["start"].isNull()) {
    any = true;
    const long sec = body["start"] | 0L;
    if (!body["start"].is<long>() || sec < 1 || sec > 86399) { error = F("start: 1 to 86399 seconds"); return 400; }
    totalMs_ = (uint32_t)sec * 1000;
    endAt_ = millis() + totalMs_;
    state_ = 1;
    overlay_ = body["show"] | true;
    const char* label = body["label"] | "";
    strlcpy(label_, label, sizeof(label_));
  }
  if (body["pause"] | false) {
    any = true;
    if (state_ == 1) { pausedLeft_ = leftMs(); state_ = 2; }
  }
  if (body["resume"] | false) {
    any = true;
    if (state_ == 2) { endAt_ = millis() + pausedLeft_; state_ = 1; }
  }
  if (!body["add"].isNull()) {
    any = true;
    const long sec = body["add"] | 0L;
    if (state_ == 0 || sec < -86399 || sec > 86399) { error = F("add: needs a running timer"); return 400; }
    const int64_t left = (int64_t)leftMs() + sec * 1000;
    const uint32_t l = left < 1000 ? 1000 : (uint32_t)left;
    if (l > totalMs_) totalMs_ = l;
    if (state_ == 1) endAt_ = millis() + l; else pausedLeft_ = l;
  }
  if (body["cancel"] | false) { any = true; state_ = 0; overlay_ = false; }
  if (body["hide"] | false) { any = true; overlay_ = false; }
  if (!body["show"].isNull() && body["start"].isNull()) { any = true; overlay_ = body["show"] | false; }
  if (!any) { error = F("nothing to do: start, pause, resume, add, cancel, show or hide"); return 400; }
  drawnKey_ = 0;
  return 200;
}

void TimerMode::toJson(JsonObject out) const {
  out["state"] = state_ == 1 ? "running" : state_ == 2 ? "paused" : "idle";
  out["left"] = (leftMs() + 999) / 1000;
  out["total"] = totalMs_ / 1000;
  out["label"] = label_;
  out["show"] = overlay_;
}

void TimerMode::service(const Settings& s) {
  const uint32_t left = leftMs();
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = ((left + 999) / 1000) * 64 + state_ * 8 + lang + 1;
  if (key == drawnKey_) return;
  drawnKey_ = key;
  int endH = -1, endM = 0;
  struct tm t;
  if (state_ == 1 && clockNow(t)) {
    const uint32_t at = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec + (left + 999) / 1000;
    endH = (at / 3600) % 24;
    endM = (at / 60) % 60;
  }
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  timerview::Layout l;
  timerview::layout(state_ == 1 ? timerview::State::Running : state_ == 2 ? timerview::State::Paused : timerview::State::Idle,
                    left, totalMs_, label_, endH, endM, lang, l);
  uint16_t row[240];
  for (int y = 0; y < 240; y++) {   // every row: the app before it may have filled the edges
    timerview::renderRow(l, y, row);
    for (int x = 0; x < 240; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, 240, 1);
    if ((y & 15) == 15) yield();
  }
}
