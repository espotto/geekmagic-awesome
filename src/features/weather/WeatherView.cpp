#include "WeatherView.h"
#include "../theme/Languages.h"
#include <stdio.h>
#include "../AppDraw.h"

namespace weatherview {
namespace {

using gm::TextStyle;
using appdraw::mix;
using appdraw::rgb;

// Conditions in gm::languageCode order (en pt es fr de it ru), 12 each:
// clear, mostly clear, partly cloudy, cloudy, fog, drizzle, rain, freezing
// rain, snow, showers, snow showers, thunderstorm. ASCII outside Cyrillic:
// the system font has no accented Latin letters.
const char kCond[] PROGMEM =
  "Clear|Mostly Clear|Partly Cloudy|Cloudy|Fog|Drizzle|Rain|Freezing Rain|Snow|Showers|Snow Showers|Thunderstorm|"
  "Ясно|Преимущественно ясно|Местами облачно|Облачно|Туман|Морось|Дождь|Ледяной дождь|Снег|Ливни|Снегопад|Гроза";
// Per language: high word, low word, "now", no location, loading.
const char kWords[] PROGMEM =
  "H|L|Now|Set the location in General|Loading...|"
  "Макс.|мин.|Сейчас|Укажите место в Основных|Загрузка...";

constexpr int kCardX0 = 22, kCardX1 = 218, kCardH = 76;
const uint16_t kWhite = 0xFFFF;

// The sky: top and bottom colours by day and at night.
uint16_t sky(bool day, int y) {
  const int t = y * 255 / (appdraw::kHeight - 1);
  return day ? mix(rgb(38, 104, 196), rgb(96, 160, 228), t) : mix(rgb(10, 18, 44), rgb(36, 56, 98), t);
}

// Distance coverage of a capsule from (ax, ay) to (bx, by), radius r, all in 1/16 px.
int capsule16(int x, int y, int ax, int ay, int bx, int by, int r) {
  const int px = x * 16 + 8, py = y * 16 + 8;
  const long vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  const long vv = vx * vx + vy * vy;
  long t = vv ? (wx * vx + wy * vy) * 1024 / vv : 0;
  if (t < 0) t = 0;
  if (t > 1024) t = 1024;
  const int cx = static_cast<int>(ax + vx * t / 1024), cy = static_cast<int>(ay + vy * t / 1024);
  return appdraw::disc16(x, y, cx, cy, r);
}

int maxi(int a, int b) { return a > b ? a : b; }

// A cloud about (cx, cy) in 1/16 px, `s` the scale in 1/16 (16 = 1).
int cloud(int x, int y, int cx, int cy) {
  int a = appdraw::disc16(x, y, cx - 64, cy + 16, 64);
  a = maxi(a, appdraw::disc16(x, y, cx + 8, cy - 24, 84));
  a = maxi(a, appdraw::disc16(x, y, cx + 80, cy + 24, 56));
  a = maxi(a, capsule16(x, y, cx - 64, cy + 40, cx + 80, cy + 40, 40));
  return a;
}
int sun(int x, int y, int cx, int cy, int r) {
  int a = appdraw::disc16(x, y, cx, cy, r);
  // Eight short rays.
  static const int8_t dir[8][2] = {{0, -16}, {11, -11}, {16, 0}, {11, 11}, {0, 16}, {-11, 11}, {-16, 0}, {-11, -11}};
  for (const auto& d : dir) {
    const int i0 = r + 28, i1 = r + 60;
    a = maxi(a, capsule16(x, y, cx + d[0] * i0 / 16, cy + d[1] * i0 / 16, cx + d[0] * i1 / 16, cy + d[1] * i1 / 16, 14));
  }
  return a;
}
int moon(int x, int y, int cx, int cy, int r) {
  const int c = appdraw::disc16(x, y, cx, cy, r) - appdraw::disc16(x, y, cx + r / 2, cy - r / 3, r * 3 / 4);
  return c < 0 ? 0 : c;
}

// Draws icon `ic` centred at (cx, cy) px onto row `y`.
void icon(Icon ic, int cx, int cy, int y, uint16_t* row) {
  if (y < cy - 13 || y > cy + 13) return;
  const int X = cx * 16, Y = cy * 16;
  for (int x = cx - 13; x <= cx + 13; ++x) {
    uint16_t c = row[x];
    switch (ic) {
      case Sun: c = mix(c, rgb(255, 214, 10), sun(x, y, X, Y, 88)); break;
      case Moon: c = mix(c, rgb(242, 242, 247), moon(x, y, X, Y, 136)); break;
      case PartSun:
      case PartMoon: {
        const int b = ic == PartSun ? sun(x, y, X - 56, Y - 56, 64) : moon(x, y, X - 48, Y - 56, 96);
        c = mix(c, ic == PartSun ? rgb(255, 214, 10) : rgb(242, 242, 247), b);
        c = mix(c, kWhite, cloud(x, y, X + 16, Y + 40));
        break;
      }
      case Cloud: c = mix(c, kWhite, cloud(x, y, X, Y + 8)); break;
      case Fog:
        for (int k = -1; k <= 1; ++k)
          c = mix(c, rgb(220, 224, 232), capsule16(x, y, X - 136 + (k & 1) * 24, Y + k * 64, X + 136 - (k & 1) * 24, Y + k * 64, 18));
        break;
      default: {   // a cloud with something falling from it
        c = mix(c, kWhite, cloud(x, y, X, Y - 48));
        for (int k = -1; k <= 1; ++k) {
          const int dx = X + k * 64, dy = Y + 104;
          if (ic == Rain) c = mix(c, rgb(100, 210, 255), capsule16(x, y, dx + 12, dy - 24, dx - 12, dy + 32, 16));
          else if (ic == Snow) c = mix(c, kWhite, appdraw::disc16(x, y, dx, dy + 8, 26));
        }
        if (ic == Storm) {
          c = mix(c, rgb(255, 214, 10), capsule16(x, y, X + 24, Y + 40, X - 16, Y + 120, 18));
          c = mix(c, rgb(255, 214, 10), capsule16(x, y, X - 16, Y + 120, X + 24, Y + 120, 18));
          c = mix(c, rgb(255, 214, 10), capsule16(x, y, X + 24, Y + 120, X - 16, Y + 200, 18));
        }
      }
    }
    row[x] = c;
  }
}

void formatTemp(float t, char* out, size_t cap) {
  const int v = static_cast<int>(t < 0 ? t - 0.5f : t + 0.5f);
  char utf8[16];
  snprintf(utf8, sizeof(utf8), "%d\xC2\xB0", v);
  gm::displayTextTo(out, cap, utf8);
}

// Fits display bytes `s` into `room` px: true when it needs Caption rather
// than Body; even Caption too wide cuts the end with "...".
bool fitLine(char* s, int room) {
  if (appdraw::width(TextStyle::Body, s) <= room) return false;
  if (appdraw::width(TextStyle::Caption, s) > room) {
    size_t len = strlen(s);
    while (len && gm::styleTextWidth(TextStyle::Caption, s, len) + appdraw::width(TextStyle::Caption, "...") > room) --len;
    while (len && s[len - 1] == ' ') --len;
    memcpy(s + len, "...", 4);
  }
  return true;
}

int colCentre(int i) { return kCardX0 + (2 * i + 1) * (kCardX1 - kCardX0) / (2 * kHours); }

}  // namespace

int conditionFor(uint8_t code) {
  switch (code) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 45: case 48: return 4;
    case 51: case 53: case 55: return 5;
    case 56: case 57: case 66: case 67: return 7;
    case 61: case 63: case 65: return 6;
    case 71: case 73: case 75: case 77: return 8;
    case 80: case 81: case 82: return 9;
    case 85: case 86: return 10;
    default: return code >= 95 ? 11 : 3;
  }
}

void conditionText(uint8_t code, uint8_t lang, char* out, size_t cap) {
  if (lang >= gm::kLanguageCount) lang = 0;
  appdraw::entry(kCond, lang * 12 + conditionFor(code), out, cap);
}

Icon iconFor(uint8_t code, bool day) {
  switch (conditionFor(code)) {
    case 0: return day ? Sun : Moon;
    case 1: case 2: return day ? PartSun : PartMoon;
    case 3: return Cloud;
    case 4: return Fog;
    case 8: case 10: return Snow;
    case 11: return Storm;
    default: return Rain;
  }
}

void layout(const Data& d, Empty empty, uint8_t lang, Layout& out) {
  out = Layout();
  out.lang = lang < gm::kLanguageCount ? lang : 0;
  out.valid = d.valid;
  out.day = d.valid ? d.day : true;
  out.tempTop = 26;
  out.condTop = out.tempTop + gm::styleLineHeight(TextStyle::DisplayS);
  out.hiloTop = out.condTop + gm::styleLineHeight(TextStyle::Body);
  out.cardTop = appdraw::kHeight - appdraw::kSafe - 2 - kCardH;
  out.cardBottom = out.cardTop + kCardH;
  const int w = out.lang * 5;
  if (!d.valid) {
    char t[8] = "--";
    gm::displayTextTo(out.temp, sizeof(out.temp), "--\xC2\xB0");
    (void)t;
    appdraw::word(kWords, w + (empty == Empty::NoLocation ? 3 : 4), out.cond, sizeof(out.cond) - 3);
    out.condSmall = fitLine(out.cond, appdraw::kWidth - 2 * appdraw::kSafe);
    return;
  }
  formatTemp(d.temp, out.temp, sizeof(out.temp));
  appdraw::word(kCond, out.lang * 12 + conditionFor(d.code), out.cond, sizeof(out.cond) - 3);
  out.condSmall = fitLine(out.cond, appdraw::kWidth - 2 * appdraw::kSafe);
  char hi[8], lo[8], hw[16], lw[16];
  formatTemp(d.high, hi, sizeof(hi));
  formatTemp(d.low, lo, sizeof(lo));
  appdraw::word(kWords, w, hw, sizeof(hw));
  appdraw::word(kWords, w + 1, lw, sizeof(lw));
  snprintf(out.hilo, sizeof(out.hilo), "%s: %s   %s: %s", hw, hi, lw, lo);
  for (int i = 0; i < kHours; ++i) {
    const Hour& h = d.hours[i];
    // The current hour too, not a "Now" word: in some languages it is wider
    // than a column; the first column stays white to mark it.
    snprintf(out.hourText[i], sizeof(out.hourText[i]), "%02u", static_cast<unsigned>(h.hour % 100));
    formatTemp(h.temp, out.hourTemp[i], sizeof(out.hourTemp[i]));
    out.hourIcon[i] = iconFor(h.code, h.day);
    out.hourDay[i] = h.day;
  }
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  const uint16_t bg = sky(l.day, y);
  for (int x = 0; x < appdraw::kWidth; ++x) row[x] = bg;
  const int cx = appdraw::kWidth / 2;
  const uint16_t soft = mix(bg, kWhite, 200);   // secondary text, white at about 80 %
  appdraw::textCentered(TextStyle::DisplayS, l.temp, cx, l.tempTop, y, kWhite, row);
  if (l.condSmall) appdraw::textCentered(TextStyle::Caption, l.cond, cx, l.condTop + 3, y, kWhite, row);
  else appdraw::textCentered(TextStyle::Body, l.cond, cx, l.condTop, y, kWhite, row);
  if (l.hilo[0]) appdraw::textCentered(TextStyle::Caption, l.hilo, cx, l.hiloTop, y, soft, row);
  if (!l.valid || y < l.cardTop || y >= l.cardBottom) return;

  // The card: white over the sky, about 15 % by day and 10 % at night.
  for (int x = kCardX0; x < kCardX1; ++x) {
    const int a = appdraw::roundRect(x, y, kCardX0, l.cardTop, kCardX1, l.cardBottom, 14);
    if (a) row[x] = mix(row[x], kWhite, a * (l.day ? 40 : 26) / 255);
  }
  const uint16_t cardSoft = mix(mix(bg, kWhite, l.day ? 40 : 26), kWhite, 200);
  for (int i = 0; i < kHours; ++i) {
    const int c = colCentre(i);
    appdraw::textCentered(TextStyle::Caption, l.hourText[i], c, l.cardTop + 6, y, i ? cardSoft : kWhite, row);
    icon(static_cast<Icon>(l.hourIcon[i]), c, l.cardTop + 39, y, row);
    appdraw::textCentered(TextStyle::Caption, l.hourTemp[i], c, l.cardTop + 52, y, kWhite, row);
  }
}

}  // namespace weatherview
