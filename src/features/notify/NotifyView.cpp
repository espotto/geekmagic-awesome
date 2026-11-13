#include "NotifyView.h"
#include <string.h>
#include "../theme/SystemFont.h"

namespace notifyview {
namespace {

// The label in Headline (28 px) for up to two lines, Body (22 px) for up to
// three, otherwise Caption (17 px) for up to kMaxLines; the title in Caption.
// All from the system font.
gm::TextStyle face(int size, bool title) {
  return title || size <= 0 ? gm::TextStyle::Caption
       : size == 1          ? gm::TextStyle::Body
                            : gm::TextStyle::Headline;
}

int width(gm::TextStyle f, const char* s, int n) { return gm::styleTextWidth(f, s, static_cast<size_t>(n)); }

// Greedy word wrap of `text` into at most `maxLines` lines no wider than
// `maxWidth`. A word wider than a line is broken between letters. Returns the
// number of lines; `overflow` is set when text was left over.
int wrap(gm::TextStyle f, const char* text, int maxWidth, int maxLines,
         uint8_t* start, uint8_t* len, bool& overflow) {
  const int n = static_cast<int>(strlen(text));
  int i = 0, lines = 0;
  overflow = false;
  while (i < n && text[i] == ' ') ++i;
  while (i < n) {
    if (lines == maxLines) { overflow = true; break; }
    int end = -1, next = i;
    for (int k = i; k < n;) {
      int wordEnd = k;
      while (wordEnd < n && text[wordEnd] != ' ') ++wordEnd;
      if (width(f, text + i, wordEnd - i) > maxWidth) break;
      end = wordEnd;
      k = wordEnd;
      while (k < n && text[k] == ' ') ++k;
      next = k;
    }
    if (end < 0) {   // one word wider than the line: break it between letters
      end = i + 1;
      while (end < n && text[end] != ' ' && width(f, text + i, end + 1 - i) <= maxWidth) ++end;
      next = end;
    }
    start[lines] = static_cast<uint8_t>(i);
    len[lines] = static_cast<uint8_t>(end - i);
    ++lines;
    i = next;
    while (i < n && text[i] == ' ') ++i;
  }
  return lines;
}

inline uint16_t blend(uint16_t bg, uint16_t fg, int a /* 0..255 */) {
  if (a <= 0) return bg;
  if (a >= 255) return fg;
  const int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
  const int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
  const int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

// Draws the part of `n` display bytes that falls on row `y`, for a line of
// style `f` whose top row is `lineTop`, starting at pen position `x`.
void drawText(gm::TextStyle f, const char* s, int n, int x, int lineTop, int y,
              uint16_t color, uint16_t* row) {
  gm::GlyphView g;
  for (int i = 0; i < n; ++i) {
    gm::styleGlyph(f, static_cast<uint8_t>(s[i]), g);
    const int gy = y - (lineTop + g.top);
    if (g.width && gy >= 0 && gy < g.height) {
      for (int gx = 0; gx < g.width; ++gx) {
        const int px = x + g.left + gx;
        if (px >= 0 && px < kWidth) row[px] = blend(row[px], color, gm::glyphCoverage(g, gx, gy));
      }
    }
    x += g.advance;
  }
}

// Coverage (0-255) of pixel (x, y) by a rounded rectangle [x0, x1) x [y0, y1)
// with corner radius r, from the distance of the pixel centre to the corner
// arc; one pixel of soft edge.
int roundRect(int x, int y, int x0, int y0, int x1, int y1, int r) {
  if (x < x0 || x >= x1 || y < y0 || y >= y1) return 0;
  const int cx = x < x0 + r ? x0 + r : x >= x1 - r ? x1 - r : -1;
  const int cy = y < y0 + r ? y0 + r : y >= y1 - r ? y1 - r : -1;
  if (cx < 0 || cy < 0) return 255;
  const int dx = 2 * x + 1 - 2 * cx, dy = 2 * y + 1 - 2 * cy;   // half pixels
  const int d2 = dx * dx + dy * dy, rIn = 2 * r - 1, rOut = 2 * r + 1;
  if (d2 <= rIn * rIn) return 255;
  if (d2 >= rOut * rOut) return 0;
  return 255 * (rOut * rOut - d2) / (rOut * rOut - rIn * rIn);
}

// Black or white, whichever reads better on `c`.
uint16_t onColor(uint16_t c) {
  const int r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
  return (r * 299 + g * 587 + b * 114) / 1000 > 160 ? 0x0000 : 0xFFFF;
}

}  // namespace

int textWidth(const char* s, int n, int size, bool title) { return width(face(size, title), s, n); }

void layout(const char* title, const char* label, Layout& out) {
  out = Layout();
  if (!label || !*label) {   // no label: the title takes its place
    label = title ? title : "";
    title = "";
  }
  out.title = title ? title : "";
  out.label = label;
  const int maxWidth = kTextW;
  bool overflow = false;
  static const uint8_t kLines[3] = {kMaxLines, 3, 2};   // per size, Caption to Headline
  for (int size = 2; size >= 0; --size) {
    out.size = static_cast<uint8_t>(size);
    out.lines = static_cast<uint8_t>(wrap(face(size, false), label, maxWidth, kLines[size], out.start, out.len, overflow));
    if (!overflow) break;
  }
  const gm::TextStyle f = face(out.size, false);
  for (int i = 0; i < out.lines; ++i) out.width[i] = static_cast<int16_t>(width(f, label + out.start[i], out.len[i]));
  if (overflow && out.lines) {   // cut the last line and mark it
    const int last = out.lines - 1, dots = width(f, "...", 3);
    while (out.len[last] && out.width[last] + dots > maxWidth) {
      --out.len[last];
      out.width[last] = static_cast<int16_t>(width(f, label + out.start[last], out.len[last]));
    }
    while (out.len[last] && label[out.start[last] + out.len[last] - 1] == ' ') --out.len[last];
    out.width[last] = static_cast<int16_t>(width(f, label + out.start[last], out.len[last]) + dots);
    out.ellipsis = true;
  }
  // The card: padding, the header (icon and title), the label lines, padding.
  const int lines = out.lines ? 8 + out.lines * gm::styleLineHeight(f) : 0;
  const int card = kPad + kIcon + lines + kPad;
  out.top = static_cast<int16_t>((kHeight - (card + kProgressGap + kProgressH)) / 2);
  out.labelTop = static_cast<int16_t>(out.top + kPad + kIcon + 8);
  out.bottom = static_cast<int16_t>(out.top + card);
  out.progressX = static_cast<int16_t>((kWidth - kProgressW) / 2);
  out.progressY = static_cast<int16_t>(out.bottom + kProgressGap);
}

bool renderRow(const Layout& l, uint16_t accent, int y, uint16_t* row) {
  if (y < l.top || y >= l.bottom) return false;
  for (int x = 0; x < kWidth; ++x)
    row[x] = blend(0, kCardColor, roundRect(x, y, kCardX, l.top, kCardX + kCardW, l.bottom, kCardRadius));
  // The header: the icon, with the title's first letter, and the title.
  const gm::TextStyle title = gm::TextStyle::Caption;
  const int iconX = kCardX + kPad, iconY = l.top + kPad;
  if (y >= iconY && y < iconY + kIcon) {
    for (int x = iconX; x < iconX + kIcon; ++x)
      row[x] = blend(row[x], accent, roundRect(x, y, iconX, iconY, iconX + kIcon, iconY + kIcon, 6));
    if (*l.title) {
      const int w = width(title, l.title, 1);
      drawText(title, l.title, 1, iconX + (kIcon - w) / 2,
               iconY + (kIcon - gm::styleLineHeight(title)) / 2, y, onColor(accent), row);
    }
  }
  if (*l.title) {
    const int lh = gm::styleLineHeight(title), top = iconY + (kIcon - lh) / 2;
    if (y >= top - lh / 4 && y < top + lh + lh / 4) {
      // Cut to the card, so a long title ends at the padding rather than past it.
      const int room = kCardX + kCardW - kPad - (iconX + kIcon + 8);
      int n = static_cast<int>(strlen(l.title));
      while (n && width(title, l.title, n) > room) --n;
      drawText(title, l.title, n, iconX + kIcon + 8, top, y, kTitleColor, row);
    }
  }
  const gm::TextStyle f = face(l.size, false);
  const int lh = gm::styleLineHeight(f);
  const int line = y >= l.labelTop ? (y - l.labelTop) / lh : -1;
  // A glyph may reach past its own line (Й, descenders), so the neighbours are drawn too.
  for (int i = line - 1; i <= line + 1; ++i) {
    if (i < 0 || i >= l.lines) continue;
    const int lineTop = l.labelTop + i * lh, x0 = kCardX + kPad;
    drawText(f, l.label + l.start[i], l.len[i], x0, lineTop, y, kLabelColor, row);
    if (l.ellipsis && i == l.lines - 1)
      drawText(f, "...", 3, x0 + width(f, l.label + l.start[i], l.len[i]), lineTop, y, kLabelColor, row);
  }
  return true;
}

}  // namespace notifyview
