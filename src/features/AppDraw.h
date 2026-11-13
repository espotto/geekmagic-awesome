// AppDraw.h — the small drawing kit the firmware's own apps share (weather,
// timer, nightstand): blending, text in the system font, discs and rings with
// a soft edge, and '|'-separated word tables kept in flash. Header-only and
// free of Arduino, so every app's view builds on the desktop for its tests.
#pragma once
#include <stdint.h>
#include <string.h>
#include "theme/SystemFont.h"
#if defined(ARDUINO)
#include <pgmspace.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#endif
#endif

namespace appdraw {

constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kSafe = 20;

inline uint16_t mix(uint16_t bg, uint16_t fg, int a) {
  if (a <= 0) return bg;
  if (a >= 255) return fg;
  const int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
  const int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
  const int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
inline uint16_t rgb(int r, int g, int b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

inline int width(gm::TextStyle st, const char* s) { return gm::styleTextWidth(st, s, strlen(s)); }

// Display bytes `s` with its line top at `lineTop`, left edge `x`, onto row `y`.
inline void text(gm::TextStyle st, const char* s, int x, int lineTop, int y, uint16_t color, uint16_t* row) {
  gm::GlyphView g;
  for (; *s; ++s) {
    gm::styleGlyph(st, static_cast<uint8_t>(*s), g);
    const int gy = y - lineTop - g.top;
    if (g.width && gy >= 0 && gy < g.height)
      for (int gx = 0; gx < g.width; ++gx) {
        const int px = x + g.left + gx;
        if (px >= 0 && px < kWidth) row[px] = mix(row[px], color, gm::glyphCoverage(g, gx, gy));
      }
    x += g.advance;
  }
}
// Cuts display bytes `s` (a buffer of `cap`) to at most `maxW` px in `st`,
// ending in "..." when anything was cut (no space before the dots).
inline void cutToWidth(gm::TextStyle st, char* s, size_t cap, int maxW) {
  if (cap < 4 || width(st, s) <= maxW) return;
  size_t n = strlen(s);
  if (n > cap - 4) n = cap - 4;
  for (;;) {
    while (n && s[n - 1] == ' ') --n;
    memcpy(s + n, "...", 4);
    if (!n || width(st, s) <= maxW) return;
    --n;
  }
}
// Centred on `cx`; true when row `y` can touch the line (so callers can skip).
inline bool lineNear(gm::TextStyle st, int lineTop, int y) {
  const int lh = gm::styleLineHeight(st);
  return y >= lineTop - lh / 4 && y < lineTop + lh + lh / 4;
}
inline void textCentered(gm::TextStyle st, const char* s, int cx, int lineTop, int y, uint16_t color, uint16_t* row) {
  if (lineNear(st, lineTop, y)) text(st, s, cx - width(st, s) / 2, lineTop, y, color, row);
}

// Coverage 0-255 of the pixel (x, y) by a disc of radius r (in 1/16 px) about
// (cx, cy) (1/16 px): one pixel of soft edge.
inline int disc16(int x, int y, int cx, int cy, int r) {
  const int dx = x * 16 + 8 - cx, dy = y * 16 + 8 - cy;
  const int d2 = dx * dx + dy * dy, a = r - 8, b = r + 8;
  if (a > 0 && d2 <= a * a) return 255;
  if (d2 >= b * b) return 0;
  // Linear in distance across the edge, from the squared distance.
  int d = 0;
  for (int s = 1 << 14; s; s >>= 1) if ((d + s) * (d + s) <= d2) d += s;   // integer sqrt
  const int c = (b - d) * 255 / 16;
  return c < 0 ? 0 : c > 255 ? 255 : c;
}
inline int ring16(int x, int y, int cx, int cy, int rIn, int rOut) {
  const int c = disc16(x, y, cx, cy, rOut) - disc16(x, y, cx, cy, rIn);
  return c < 0 ? 0 : c;
}

// Coverage of a rounded rectangle [x0, x1) x [y0, y1), corner radius r (px).
inline int roundRect(int x, int y, int x0, int y0, int x1, int y1, int r) {
  if (x < x0 || x >= x1 || y < y0 || y >= y1) return 0;
  const int cx = x < x0 + r ? x0 + r : x >= x1 - r ? x1 - r : -1;
  const int cy = y < y0 + r ? y0 + r : y >= y1 - r ? y1 - r : -1;
  if (cx < 0 || cy < 0) return 255;
  return disc16(x, y, cx * 16, cy * 16, r * 16);
}

// The `index`th '|'-separated entry of a flash table, UTF-8, into `out`.
inline void entry(const char* table, int index, char* out, size_t cap) {
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
// The same entry as display bytes (Cyrillic and ° become single bytes).
inline void word(const char* table, int index, char* out, size_t cap) {
  char utf8[64];
  entry(table, index, utf8, sizeof(utf8));
  gm::displayTextTo(out, cap, utf8);
}

}  // namespace appdraw
