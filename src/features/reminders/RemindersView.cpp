#include "RemindersView.h"
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

namespace remindersview {
namespace {

using gm::TextStyle;

// Words in gm::languageCode order (en pt es fr de it ru), '|'-separated
// UTF-8 in flash: on the ESP8266 a plain string table would sit in RAM.
const char kTitles[] PROGMEM =
  "Reminders|"
  "Напоминания";
const char kEmpty[] PROGMEM =
  "No reminders|"
  "Нет напоминаний";
const char kMore[] PROGMEM =
  "more|"
  "ещё";

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

// Coverage of a pixel by the ring between radii rIn and rOut (rIn 0: a disc),
// in half pixels from the centre; one pixel of soft edge on each side.
int ring(int dx2, int dy2, int rIn, int rOut) {
  const int d2 = dx2 * dx2 + dy2 * dy2;
  auto inside = [d2](int r) {   // coverage of a disc of radius r (half pixels)
    const int a = r - 1, b = r + 1;
    if (r <= 0) return 0;
    if (d2 <= a * a) return 255;
    if (d2 >= b * b) return 0;
    return 255 * (b * b - d2) / (b * b - a * a);
  };
  const int c = inside(rOut) - inside(rIn);
  return c < 0 ? 0 : c;
}

// Cuts display bytes `s` to `room` px with "...".
void fitText(char* s, TextStyle st, int room) {
  if (width(st, s) <= room) return;
  size_t len = strlen(s);
  const int dots = width(st, "...");
  while (len && gm::styleTextWidth(st, s, len) + dots > room) --len;
  while (len && s[len - 1] == ' ') --len;
  memcpy(s + len, "...", 4);
}

constexpr int kLeft = kSafe + 4;              // the rings' left edge
constexpr int kRingR = 9;
constexpr int kTextX = kLeft + 2 * kRingR + 10;
constexpr int kRight = kWidth - kSafe - 4;

}  // namespace

void layout(const reminders::List& list, uint8_t lang, Layout& out) {
  out = Layout();
  out.lang = lang < gm::kLanguageCount ? lang : 0;
  char utf8[64];
  entry(kTitles, out.lang, utf8, sizeof(utf8));
  gm::displayTextTo(out.title, sizeof(out.title), utf8);
  snprintf(out.count, sizeof(out.count), "%d", list.open());
  entry(kEmpty, out.lang, utf8, sizeof(utf8));
  gm::displayTextTo(out.empty, sizeof(out.empty), utf8);
  out.smallTitle = width(TextStyle::Headline, out.title) + 12 + width(TextStyle::Headline, out.count) > kRight - kLeft;

  out.titleTop = kSafe;
  out.listTop = out.titleTop + gm::styleLineHeight(TextStyle::Headline) + 8;
  const int capacity = (kHeight - kSafe - out.listTop) / kRowH;

  int rows = 0;
  for (int pass = 0; pass < 2; ++pass)   // open items first, then the done ones
    for (int i = 0; i < list.count(); ++i)
      if (list.at(i).done == (pass == 1)) out.order[rows++] = i;
  out.shown = rows <= capacity ? rows : capacity - 1;
  out.more = rows - out.shown;
  if (out.more) {
    char word[24];
    entry(kMore, out.lang, word, sizeof(word));
    char m[40];
    snprintf(m, sizeof(m), "+%d %s", out.more, word);
    gm::displayTextTo(out.moreText, sizeof(out.moreText), m);
  }
  // One size for the whole list, as in the app: Body when every row fits,
  // else Caption for all, a row cut with "..." only if even that is too long.
  bool small = false;
  for (int r = 0; r < out.shown; ++r) {
    const reminders::Item& it = list.at(out.order[r]);
    gm::displayTextTo(out.text[r], sizeof(out.text[r]) - 3, it.text);
    if (width(TextStyle::Body, out.text[r]) > kRight - kTextX) small = true;
    out.done[r] = it.done;
  }
  for (int r = 0; r < out.shown; ++r) {
    out.small[r] = small;
    fitText(out.text[r], small ? TextStyle::Caption : TextStyle::Body, kRight - kTextX);
  }
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  for (int x = 0; x < kWidth; ++x) row[x] = 0;

  // The title in blue, the open count at the right in the same colour.
  const int hh = gm::styleLineHeight(TextStyle::Headline);
  if (y >= l.titleTop - 4 && y < l.titleTop + hh + hh / 4) {
    const int base = l.titleTop + gm::styleBaseline(TextStyle::Headline);
    const TextStyle ts = l.smallTitle ? TextStyle::Body : TextStyle::Headline;
    drawText(ts, l.title, kLeft, base - gm::styleBaseline(ts), y, kBlue, row);
    drawText(ts, l.count, kRight - width(ts, l.count), base - gm::styleBaseline(ts), y, kBlue, row);
  }

  const int bh = gm::styleLineHeight(TextStyle::Body);
  if (!l.shown && !l.more) {
    const int top = l.listTop + 6;
    if (y >= top - 4 && y < top + bh + 4) drawText(TextStyle::Body, l.empty, kLeft, top, y, kGrey, row);
    return;
  }
  if (y < l.listTop) return;
  const int band = (y - l.listTop) / kRowH;
  for (int r = band - 1; r <= band + 1; ++r) {
    if (r < 0 || r > l.shown) continue;
    const int top = l.listTop + r * kRowH, cy = top + kRowH / 2;
    if (r == l.shown) {   // "+N more" in place of a last row
      if (l.more && y >= top && y < top + kRowH)
        drawText(TextStyle::Caption, l.moreText, kTextX, cy - gm::styleLineHeight(TextStyle::Caption) / 2, y, kGrey, row);
      continue;
    }
    // The ring: open is a grey outline; done is a blue outline round a blue dot.
    if (y >= cy - kRingR - 1 && y <= cy + kRingR + 1) {
      const int cx = kLeft + kRingR;
      for (int x = cx - kRingR - 1; x <= cx + kRingR + 1; ++x) {
        const int dx2 = 2 * (x - cx) + 1 - 1, dy2 = 2 * (y - cy) + 1 - 1;   // centre on a pixel
        int a = ring(dx2, dy2, 2 * kRingR - 4, 2 * kRingR);
        if (l.done[r]) a += ring(dx2, dy2, 0, 2 * kRingR - 8);
        row[x] = mix(row[x], l.done[r] ? kBlue : kRing, a > 255 ? 255 : a);
      }
    }
    const TextStyle st = l.small[r] ? TextStyle::Caption : TextStyle::Body;
    const int th = gm::styleLineHeight(st), tt = cy - th / 2;
    if (y >= tt - th / 4 && y < tt + th + th / 4)
      drawText(st, l.text[r], kTextX, tt, y, l.done[r] ? kGrey : kText, row);
    // A hairline under every row but the last, from the text's edge, as in the app.
    if (y == top + kRowH - 1 && (r + 1 < l.shown || l.more))
      for (int x = kTextX; x < kRight; ++x) row[x] = kLine;
  }
}

}  // namespace remindersview
