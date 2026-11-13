#include "CalendarView.h"
#include "../theme/Languages.h"
#include <stdio.h>
#include <string.h>
#include "../theme/SystemFont.h"
#if defined(ARDUINO)
#include <pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#endif

namespace calendarview {
namespace {

using gm::TextStyle;

// Month names as a heading says them (nominative, capitalized), and the
// weekday initials in the order the week starts: Sunday for English, Monday
// otherwise. Languages in gm::languageCode order (en pt es fr de it ru),
// as '|'-separated UTF-8 in flash: on the ESP8266 a plain string table would
// sit in RAM, about 1 KB of the heap.
const char kMonths[] PROGMEM =
  "January|February|March|April|May|June|July|August|September|October|November|December|"
  "Январь|Февраль|Март|Апрель|Май|Июнь|Июль|Август|Сентябрь|Октябрь|Ноябрь|Декабрь";
const char kInitials[] PROGMEM =
  "S|M|T|W|T|F|S|"
  "П|В|С|Ч|П|С|В";

// The `index`th '|'-separated entry of a flash table, UTF-8, into `out`.
void entry(const char* table, int index, char* out, size_t cap) {
  size_t n = 0;
  for (const char* p = table;; ++p) {
    const char c = static_cast<char>(pgm_read_byte(p));
    if (c == '|' || c == 0) {
      if (index-- == 0 || c == 0) break;
      continue;
    }
    if (index == 0 && n + 1 < cap) out[n++] = c;
  }
  if (cap) out[n] = 0;
}

inline bool sundayFirst(uint8_t lang) { return lang == 0; }

inline uint16_t mix(uint16_t bg, uint16_t fg, int a) {
  if (a <= 0) return bg;
  if (a >= 255) return fg;
  const int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
  const int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
  const int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

int width(TextStyle style, const char* s) { return gm::styleTextWidth(style, s, strlen(s)); }

void drawText(TextStyle style, const char* s, int x, int lineTop, int y, uint16_t color, uint16_t* row) {
  gm::GlyphView g;
  for (; *s; ++s) {
    gm::styleGlyph(style, static_cast<uint8_t>(*s), g);
    const int gy = y - lineTop - g.top;
    if (g.width && gy >= 0 && gy < g.height)
      for (int gx = 0; gx < g.width; ++gx) {
        const int px = x + g.left + gx;
        if (px >= 0 && px < kWidth) row[px] = mix(row[px], color, gm::glyphCoverage(g, gx, gy));
      }
    x += g.advance;
  }
}

// A filled circle of radius r about (cx, cy), one pixel of soft edge.
void drawDisc(int cx, int cy, int r, int y, uint16_t color, uint16_t* row) {
  const int dy = 2 * (y - cy) + 1;   // half pixels, pixel centres
  for (int x = cx - r - 1; x <= cx + r; ++x) {
    if (x < 0 || x >= kWidth) continue;
    const int dx = 2 * (x - cx) + 1;
    const int d2 = dx * dx + dy * dy, rIn = 2 * r - 1, rOut = 2 * r + 1;
    const int a = d2 <= rIn * rIn ? 255 : d2 >= rOut * rOut ? 0 : 255 * (rOut * rOut - d2) / (rOut * rOut - rIn * rIn);
    row[x] = mix(row[x], color, a);
  }
}

int colCentre(int c) { return kSafe + (2 * c + 1) * (kWidth - 2 * kSafe) / 14; }

// Weekend columns: Saturday and Sunday wherever the week starts.
bool weekend(const Layout& l, int col) {
  const int wd = sundayFirst(l.lang) ? col : (col + 1) % 7;   // 0 = Sunday
  return wd == 0 || wd == 6;
}

}  // namespace

int daysIn(int year, int month) {
  static const uint8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return month >= 1 && month <= 12 ? d[month - 1] : 30;
}

int weekday(int year, int month, int day) {   // Sakamoto's method
  static const uint8_t t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

void layout(int year, int month, int today, uint8_t lang, Layout& out) {
  out = Layout();
  if (month < 1 || month > 12) month = 1;
  out.year = year;
  out.month = month;
  out.lang = lang < gm::kLanguageCount ? lang : 0;
  out.days = daysIn(year, month);
  out.today = today >= 1 && today <= out.days ? today : 0;
  const int wd1 = weekday(year, month, 1);
  out.firstCol = sundayFirst(out.lang) ? wd1 : (wd1 + 6) % 7;
  out.rows = (out.firstCol + out.days + 6) / 7;
  char utf8[32];
  entry(kMonths, out.lang * 12 + month - 1, utf8, sizeof(utf8));
  gm::displayTextTo(out.title, sizeof(out.title), utf8);
  snprintf(out.yearText, sizeof(out.yearText), "%d", year);
  const int room = colCentre(6) - colCentre(0) + 16 - 10;
  out.smallTitle = width(TextStyle::Headline, out.title) + width(TextStyle::Body, out.yearText) > room;

  const int head = gm::styleLineHeight(TextStyle::Headline);
  const int initials = gm::styleLineHeight(TextStyle::Caption);
  const int fixed = head + 6 + initials + 4;
  out.rowH = (kHeight - 2 * kSafe - fixed) / out.rows;
  if (out.rowH > 28) out.rowH = 28;
  const int block = fixed + out.rows * out.rowH;
  out.top = (kHeight - block) / 2;
  out.headTop = out.top;
  out.initialsTop = out.top + head + 6;
  out.gridTop = out.initialsTop + initials + 4;
  out.bottom = out.top + block;
}

void cellCentre(const Layout& l, int day, int& x, int& y) {
  const int i = l.firstCol + day - 1;
  x = colCentre(i % 7);
  y = l.gridTop + (i / 7) * l.rowH + l.rowH / 2;
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  for (int x = 0; x < kWidth; ++x) row[x] = 0;
  if (y < l.top - 4 || y >= l.bottom + 4) return;

  // The month in red at the grid's left edge, the year in grey at its right.
  const int head = gm::styleLineHeight(TextStyle::Headline);
  if (y < l.headTop + head + head / 4) {
    // Both on the Headline baseline; the year always in Body, the month in
    // Body too when it would otherwise reach the year.
    const int left = colCentre(0) - 8, base = l.headTop + gm::styleBaseline(TextStyle::Headline);
    const TextStyle ts = l.smallTitle ? TextStyle::Body : TextStyle::Headline;
    drawText(ts, l.title, left, base - gm::styleBaseline(ts), y, kRed, row);
    const int yw = width(TextStyle::Body, l.yearText);
    drawText(TextStyle::Body, l.yearText, colCentre(6) + 8 - yw, base - gm::styleBaseline(TextStyle::Body), y, kGrey, row);
  }

  const TextStyle st = TextStyle::Caption;
  const int lh = gm::styleLineHeight(st);
  if (y >= l.initialsTop - lh / 4 && y < l.initialsTop + lh + lh / 4) {
    char s[4], utf8[4];
    for (int c = 0; c < 7; ++c) {
      entry(kInitials, l.lang * 7 + c, utf8, sizeof(utf8));
      gm::displayTextTo(s, sizeof(s), utf8);
      drawText(st, s, colCentre(c) - width(st, s) / 2, l.initialsTop, y, kDim, row);
    }
  }

  if (y < l.gridTop - 2) return;
  const int r = (l.rowH - 2) / 2 < 11 ? (l.rowH - 2) / 2 : 11;
  const int line = (y - l.gridTop) / l.rowH;
  // A circle or a glyph may reach into the next band: check the neighbours.
  for (int band = line - 1; band <= line + 1; ++band) {
    if (band < 0 || band >= l.rows) continue;
    for (int c = 0; c < 7; ++c) {
      const int day = band * 7 + c - l.firstCol + 1;
      if (day < 1 || day > l.days) continue;
      const int cx = colCentre(c), cy = l.gridTop + band * l.rowH + l.rowH / 2;
      const bool today = day == l.today;
      if (today && y >= cy - r - 1 && y <= cy + r) drawDisc(cx, cy, r, y, kRed, row);
      char s[12];
      snprintf(s, sizeof(s), "%d", day);
      const int top = cy - lh / 2;
      if (y >= top - lh / 4 && y < top + lh + lh / 4)
        drawText(st, s, cx - width(st, s) / 2, top, y, today ? 0xFFFF : weekend(l, c) ? kGrey : kText, row);
    }
  }
}

}  // namespace calendarview
