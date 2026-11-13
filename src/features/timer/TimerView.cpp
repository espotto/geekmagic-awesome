#include "TimerView.h"
#include <math.h>
#include <stdio.h>
#include "../AppDraw.h"

namespace timerview {
namespace {

using gm::TextStyle;
using appdraw::mix;
using appdraw::rgb;

// Per language (en pt es fr de it ru): "Timer", "Ends %s", "Paused", "Time's up".
const char kWords[] PROGMEM =
  "Timer|Ends %s|Paused|Time's up|"
  "Таймер|До %s|Пауза|Время вышло";

constexpr int kCx = 120, kCy = 120, kOuter = 98, kWidthPx = 12;
constexpr int kMid16 = (kOuter - kWidthPx / 2) * 16;
const uint16_t kOrange = 0xFCE1;      // #ff9f0a
const uint16_t kOrangeDim = 0x7A40;   // a paused arc
const uint16_t kTrack = 0x31A6;       // #333333 with a warm cast
const uint16_t kGrey = 0x8C72;

// A pixel's direction relative to the start (12 o'clock) and the end, by
// cross products: no trigonometry per pixel.
bool inArc(const Layout& l, int dx, int dy) {
  if (l.sweep >= 360) return true;
  if (l.sweep <= 0) return false;
  // cross(A, B) = A.x*B.y - A.y*B.x; with y down, positive means B is clockwise of A.
  const long cs = (long)0 * dy - (long)(-1) * dx;          // start S = (0, -1)
  const long ce = (long)l.ex * dy - (long)l.ey * dx;       // end E
  if (l.sweep <= 180) return cs >= 0 && ce <= 0;
  return !(ce > 0 && cs < 0);   // outside only in the complement [sweep, 360)
}

}  // namespace

void layout(State st, uint32_t leftMs, uint32_t totalMs, const char* label,
            int endHour, int endMin, uint8_t lang, Layout& out) {
  out = Layout();
  out.state = st;
  if (lang > 6) lang = 0;
  const uint32_t left = (leftMs + 999) / 1000;   // a second shows until it has fully passed
  const unsigned h = left / 3600, m = (left / 60) % 60, s = left % 60;
  char t[16];
  if (h) snprintf(t, sizeof(t), "%u:%02u:%02u", h, m, s);
  else snprintf(t, sizeof(t), "%02u:%02u", m, s);
  gm::displayTextTo(out.digits, sizeof(out.digits), t);
  out.smallDigits = appdraw::width(TextStyle::DisplayS, out.digits) > 2 * (kOuter - kWidthPx) - 16;

  out.sweep = st == State::Idle ? 360 : totalMs ? (int)((uint64_t)leftMs * 360 / totalMs) : 0;
  if (out.sweep > 360) out.sweep = 360;
  const float a = out.sweep * 3.14159265f / 180.0f;
  out.ex = (int)lroundf(sinf(a) * 1024);
  out.ey = (int)lroundf(-cosf(a) * 1024);
  out.endX = kCx * 16 + 8 + out.ex * kMid16 / 1024 - 8;
  out.endY = kCy * 16 + 8 + out.ey * kMid16 / 1024 - 8;

  char w[40];
  if (label && *label) gm::displayTextTo(out.label, sizeof(out.label), label);
  else appdraw::word(kWords, lang * 4, out.label, sizeof(out.label));
  if (st == State::Paused) appdraw::word(kWords, lang * 4 + 2, out.sub, sizeof(out.sub));
  else if (st == State::Running && endHour >= 0) {
    appdraw::entry(kWords, lang * 4 + 1, w, sizeof(w));
    char clock[24], utf8[48];
    snprintf(clock, sizeof(clock), "%02d:%02d", endHour, endMin);
    snprintf(utf8, sizeof(utf8), w, clock);
    gm::displayTextTo(out.sub, sizeof(out.sub), utf8);
  }
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  for (int x = 0; x < appdraw::kWidth; ++x) row[x] = 0;
  const int dy = y - kCy;
  if (dy >= -kOuter - 1 && dy <= kOuter + 1) {
    const uint16_t arc = l.state == State::Paused ? kOrangeDim : l.state == State::Idle ? kTrack : kOrange;
    const int capR = kWidthPx * 8;   // half the band, 1/16 px
    for (int x = kCx - kOuter - 1; x <= kCx + kOuter + 1; ++x) {
      const int a = appdraw::ring16(x, y, kCx * 16, kCy * 16, (kOuter - kWidthPx) * 16, kOuter * 16);
      int on = 0;
      if (a) on = inArc(l, x - kCx, dy) ? a : 0;
      if (l.state != State::Idle && l.sweep > 0 && l.sweep < 360) {   // round ends
        const int c0 = appdraw::disc16(x, y, kCx * 16, kCy * 16 - kMid16, capR);
        const int c1 = appdraw::disc16(x, y, l.endX, l.endY, capR);
        if (c0 > on) on = c0;
        if (c1 > on) on = c1;
      }
      if (!a && !on) continue;
      uint16_t c = mix(row[x], kTrack, a);
      row[x] = mix(c, arc, on);
    }
  }
  const TextStyle ds = l.smallDigits ? TextStyle::Headline : TextStyle::DisplayS;
  const int dh = gm::styleLineHeight(ds);
  appdraw::textCentered(ds, l.digits, kCx, kCy - dh / 2 - 2, y, 0xFFFF, row);
  appdraw::textCentered(TextStyle::Caption, l.label, kCx, kCy - dh / 2 - 26, y, kGrey, row);
  if (l.sub[0]) appdraw::textCentered(TextStyle::Caption, l.sub, kCx, kCy + dh / 2 + 4, y, l.state == State::Paused ? kOrange : kGrey, row);
}

void bannerWords(uint8_t lang, char* title, int titleCap, char* text, int textCap) {
  if (lang > 6) lang = 0;
  appdraw::entry(kWords, lang * 4, title, titleCap);
  appdraw::entry(kWords, lang * 4 + 3, text, textCap);
}

}  // namespace timerview
