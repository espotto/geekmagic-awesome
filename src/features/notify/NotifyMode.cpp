#include "NotifyMode.h"
#include <Arduino_GFX_Library.h>
#include <stdlib.h>
#include <string.h>
#include "Gfx.h"
#include "SysText.h"
#include "NotifyTypes.h"
#include "NotifyView.h"

NotifyMode g_notifyMode;

// The banner (NotifyView): the card is drawn once per item, sliding in, then
// only the time-left line under it changes.

// "#rrggbb", "rrggbb", or one of the names below, to RGB565. Names exist so a
// shell one-liner does not have to know the 565 packing; hex exists so anything
// else is reachable.
static bool parseColor(const char* s, uint16_t* out) {
  static const struct { const char* name; uint16_t rgb; } kNamed[] = {
    {"white", 0xFFFF}, {"black", 0x0000}, {"red",     0xF800}, {"green", 0x07E0},
    {"blue",  0x041F}, {"yellow", 0xFFE0}, {"cyan",   0x07FF}, {"magenta", 0xF81F},
    {"orange", 0xFC00}, {"gray",  0x8410}, {"grey",   0x8410},
  };
  for (size_t i = 0; i < sizeof(kNamed) / sizeof(kNamed[0]); i++) {
    if (!strcasecmp(s, kNamed[i].name)) { *out = kNamed[i].rgb; return true; }
  }
  if (*s == '#') s++;

  // Six hex digits, checked one by one. strtoul is not a validator: it skips
  // leading whitespace and accepts a sign, so " 12345" and "-00001" both clear a
  // length test and then parse into a colour nobody asked for.
  unsigned long v = 0;
  for (int i = 0; i < 6; i++) {
    const char c = s[i];
    int d;
    if      (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return false;
    v = (v << 4) | (unsigned long)d;
  }
  if (s[6] != '\0') return false;

  *out = (uint16_t)((((v >> 19) & 0x1F) << 11) | (((v >> 10) & 0x3F) << 5) |
                    ((v >> 3) & 0x1F));
  return true;
}

// The animation names the endpoint used to accept, kept only for validation.
static bool knownAnim(const char* name) {
  static const char* const kNames[] = {"info", "warn", "bang", "pulse", "jumping", "waving", "done", "waiting"};
  for (const char* n : kNames) if (!strcasecmp(name, n)) return true;
  return false;
}

// ---- rendering ------------------------------------------------------------

// The card, composed a row at a time and tinted like every other screen. On a
// full repaint it drops in from a little above its place while fading up, the
// way a banner arrives on a phone: six frames, each drawing the card at its
// new offset and clearing the rows it left behind. No frame buffer needed.
void NotifyMode::draw(bool full) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  if (full) {
    static const struct { int8_t dy; uint8_t alpha; } kFrames[] = {
      {-24, 70}, {-13, 140}, {-6, 195}, {-3, 230}, {-1, 248}, {0, 255},
    };
    gfx->fillScreen(C_BLACK);
    uint16_t row[notifyview::kWidth];
    int prevTop = layout_.top + kFrames[0].dy;
    for (const auto& f : kFrames) {
      const uint32_t t0 = millis();
      const int top = layout_.top + f.dy;
      if (top > prevTop) gfx->fillRect(0, prevTop, notifyview::kWidth, top - prevTop, C_BLACK);
      prevTop = top;
      for (int y = layout_.top; y < layout_.bottom; y++) {
        if (!notifyview::renderRow(layout_, cur_.color, y, row)) continue;
        for (int x = 0; x < notifyview::kWidth; x++) {
          uint16_t c = row[x];
          if (f.alpha < 255 && c) {   // fade towards black
            const uint16_t r = (c >> 11) * f.alpha / 255, g = ((c >> 5) & 63) * f.alpha / 255, b = (c & 31) * f.alpha / 255;
            c = (uint16_t)((r << 11) | (g << 5) | b);
          }
          row[x] = gfxTint(c);
        }
        gfx->draw16bitRGBBitmap(0, y + f.dy, row, notifyview::kWidth, 1);
        if ((y & 15) == 15) yield();
      }
      while (millis() - t0 < 30) yield();   // an even pace on short cards
    }
    gfx->fillRect(layout_.progressX, layout_.progressY, notifyview::kProgressW,
                  notifyview::kProgressH, gfxTint(notifyview::kTrackColor));
    barW_ = 0;
  }
  drawProgress();
}

// The line shrinks from full width to nothing over the item's hold time. Only
// the part that changed is drawn: the accent run on the first frame, then the
// track colour over what the line gave up.
void NotifyMode::drawProgress() {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  const uint32_t ttl = (uint32_t)cur_.ttlSec * 1000UL;
  const int32_t left = (int32_t)(untilMs_ - millis());
  int w = (left <= 0 || !ttl) ? 0 : (int)((uint64_t)left * notifyview::kProgressW / ttl);
  if (w > notifyview::kProgressW) w = notifyview::kProgressW;
  const int x = layout_.progressX, y = layout_.progressY;
  if (barW_ == 0 && w > 0) {
    gfx->fillRect(x, y, w, notifyview::kProgressH, gfxTint(cur_.color));
  } else if (w < barW_) {
    gfx->fillRect(x + w, y, barW_ - w, notifyview::kProgressH, gfxTint(notifyview::kTrackColor));
  } else {
    return;
  }
  barW_ = w;
}

// ---- queue ----------------------------------------------------------------

// The request is UTF-8; it is kept as display bytes (one per character: ASCII,
// Cyrillic, the degree sign, anything else '?'), so the system font can draw it
// and the length limits count letters, not bytes.
void NotifyMode::copyText(char* dst, size_t cap, const char* src) {
  gm::displayTextTo(dst, cap, src);
}

void NotifyMode::startItem(const NotifyItem& it) {
  cur_ = it;

  // Lay the text out once per item: it depends only on the text.
  notifyview::layout(cur_.title, cur_.label, layout_);
  barW_ = 0;

  startedMs_    = millis();
  untilMs_      = startedMs_ + (uint32_t)cur_.ttlSec * 1000UL;
  primed_       = false;

  if (!armed_) {            // the panel was not ours: this starts the run
    overlayStartedMs_ = startedMs_;
    armed_ = true;
  }
}

// Sorted highest-priority-first, FIFO among equals, so inserting is a search for
// the first entry this one outranks.
bool NotifyMode::enqueue(const NotifyItem& it) {
  if (qCount_ >= NOTIFY_QUEUE_DEPTH) {
    // Full. Evict the last entry this one outranks or ties — the queue is
    // sorted, so that is the least important thing waiting. If everything queued
    // is strictly more important, the arrival is what gives way.
    int victim = -1;
    for (int i = (int)qCount_ - 1; i >= 0; i--) {
      if (queue_[i].prio <= it.prio) { victim = i; break; }
    }
    if (victim < 0) return false;
    for (uint8_t i = (uint8_t)victim + 1; i < qCount_; i++) queue_[i - 1] = queue_[i];
    qCount_--;
  }

  uint8_t at = qCount_;
  for (uint8_t i = 0; i < qCount_; i++) {
    if (queue_[i].prio < it.prio) { at = i; break; }
  }
  for (uint8_t i = qCount_; i > at; i--) queue_[i] = queue_[i - 1];
  queue_[at] = it;
  qCount_++;
  return true;
}

void NotifyMode::advance() {
  if (qCount_ == 0) { retire(); return; }
  const NotifyItem next = queue_[0];
  for (uint8_t i = 1; i < qCount_; i++) queue_[i - 1] = queue_[i];
  qCount_--;
  startItem(next);
}

bool NotifyMode::owns() const {
  return armed_ && (qCount_ > 0 || (int32_t)(millis() - untilMs_) < 0);
}

uint32_t NotifyMode::heldMs() const {
  if (!armed_) return lastHeldMs_;
  // Cap at the current item's own expiry. When service() runs normally the two
  // are the same; when it was starved, wall clock kept moving while the panel
  // showed something else entirely, and crediting that would hand the carousel
  // back time the overlay never took.
  const uint32_t now = millis();
  const uint32_t end = ((int32_t)(now - untilMs_) >= 0) ? untilMs_ : now;
  return end - overlayStartedMs_;
}

void NotifyMode::retire() {
  if (!armed_) return;
  lastHeldMs_ = heldMs();   // freeze before clearing armed_
  armed_ = false;
}

// Two requests describe the same event when they name the same preset and carry
// the same label. That is the heartbeat case: a script re-firing every few
// seconds to say "still waiting" means refresh, not another overlay.
bool NotifyMode::sameEvent(const NotifyItem& a, const NotifyItem& b) {
  return a.type == b.type && !strcmp(a.label, b.label);
}

NotifyResult NotifyMode::request(const NotifyRequest& r) {
  int t = notifyTypeFind(r.type);
  if (t < 0) {
    if (r.type && *r.type) return NOTIFY_REJECTED;   // a type that does not exist
    t = NOTIFY_TYPE_DEFAULT;
  }
  const NotifyType& preset = notify_types[t];

  NotifyItem it;
  it.type   = (uint8_t)t;
  it.color  = preset.color;
  it.prio   = preset.prio;
  it.ttlSec = preset.ttlSec;

  // The screen has no animations any more. The names the API accepted still
  // pass, so existing scripts keep working; an unknown one is still a typo.
  if (r.anim && *r.anim && !knownAnim(r.anim)) return NOTIFY_REJECTED;
  if (r.color && *r.color) {
    uint16_t c;
    if (!parseColor(r.color, &c)) return NOTIFY_REJECTED;
    it.color = c;
  }
  if (r.prio >= 0) it.prio = (uint8_t)(r.prio > NOTIFY_PRIO_MAX ? NOTIFY_PRIO_MAX : r.prio);
  if (r.ttlSec) it.ttlSec = (uint16_t)(r.ttlSec > NOTIFY_TTL_MAX_SEC ? NOTIFY_TTL_MAX_SEC
                                                                     : r.ttlSec);
  if (it.ttlSec < NOTIFY_TTL_MIN_SEC) it.ttlSec = NOTIFY_TTL_MIN_SEC;

  copyText(it.label, sizeof(it.label), r.label);

  // The preset's word is a fallback, not a header. Printing "INFO" above a label
  // says nothing the colour and the animation have not already said, and costs a
  // line the message could use. It appears only when there is no label at all,
  // which is also what keeps a bare {"state":"done"} showing TASK DONE.
  if (r.title && *r.title)      copyText(it.title, sizeof(it.title), r.title);
  else if (it.label[0] == '\0') {   // the type's word, in the device language
    char word[40];
    copyText(it.title, sizeof(it.title), sys::text((sys::Word)(sys::NotifyInfo + t), word, sizeof(word)));
  }

  // A run that has finished but whose service() never got to clear it (safe
  // mode, setup mode) must not make this look like an overlay is up.
  if (!owns()) retire();

  // A repeat of what is on screen refreshes it in place. This is what the
  // endpoint is mostly used for: a session heartbeat re-firing the same waiting
  // notice every few seconds. Queueing those would hold the panel for the sum of
  // their hold times and then replay events the session has long moved past.
  if (armed_ && sameEvent(it, cur_)) { startItem(it); return NOTIFY_ACCEPTED; }

  // Same for a repeat of something already waiting: refresh it where it sits
  // rather than letting a heartbeat fill the queue behind a long alert.
  for (uint8_t i = 0; i < qCount_; i++) {
    if (sameEvent(it, queue_[i])) { queue_[i] = it; return NOTIFY_ACCEPTED; }
  }

  // Nothing on the panel, or something less important than this: show it now.
  // The pre-empted overlay is dropped rather than requeued — it has already had
  // its time on screen, and what arrived outranks it.
  if (!armed_ || it.prio > cur_.prio) { startItem(it); return NOTIFY_ACCEPTED; }
  return enqueue(it) ? NOTIFY_ACCEPTED : NOTIFY_QUEUE_FULL;
}

// ---- service --------------------------------------------------------------

void NotifyMode::service(const Settings& s) {
  (void)s;
  if (!armed_) return;
  if (!owns()) { retire(); return; }   // ran out while we were not being called

  if ((int32_t)(millis() - untilMs_) >= 0) {
    advance();
    if (!armed_) return;   // queue drained: main.cpp restores the feature below
  }

  if (!primed_) {
    primed_ = true;
    draw(true);
    return;
  }
  drawProgress();
}
