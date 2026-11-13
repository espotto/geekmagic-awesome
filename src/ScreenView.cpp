#include "ScreenView.h"
#include "SysText.h"
#include <stdio.h>
#include <string.h>

namespace screenview {
namespace {

using gm::TextStyle;

inline uint16_t mix(uint16_t bg, uint16_t fg, int a) {
  if (a <= 0) return bg;
  if (a >= 255) return fg;
  const int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
  const int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
  const int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

int width(TextStyle style, const char* s) { return gm::styleTextWidth(style, s, strlen(s)); }

// The largest style from `largest` down to Caption that fits `room`.
TextStyle fit(const char* s, int room, TextStyle largest) {
  for (int st = static_cast<int>(largest); st > static_cast<int>(TextStyle::Caption); --st)
    if (width(static_cast<TextStyle>(st), s) <= room) return static_cast<TextStyle>(st);
  return TextStyle::Caption;
}

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

// The accent dot: radius 4, anti-aliased by distance from the centre.
void drawDot(int cx, int cy, int y, uint16_t color, uint16_t* row) {
  const int dy2 = (y - cy) * (y - cy) * 16;
  for (int x = cx - 5; x <= cx + 5; ++x) {
    if (x < 0 || x >= kWidth) continue;
    const int d2 = (x - cx) * (x - cx) * 16 + dy2;
    const int a = d2 <= 14 * 14 ? 255 : d2 >= 18 * 18 ? 0 : 255 - (255 * (d2 - 196)) / (324 - 196);
    row[x] = mix(row[x], color, a);
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
  // In half pixels: the pixel centre is (2x+1, 2y+1), the arc centre (2cx, 2cy).
  const int dx = 2 * x + 1 - 2 * cx, dy = 2 * y + 1 - 2 * cy;
  const int d2 = dx * dx + dy * dy, rIn = 2 * r - 1, rOut = 2 * r + 1;
  if (d2 <= rIn * rIn) return 255;
  if (d2 >= rOut * rOut) return 0;
  return 255 * (rOut * rOut - d2) / (rOut * rOut - rIn * rIn);
}

}  // namespace

Span render(const Line* lines, int count, uint16_t accent, RowSink sink, void* context) {
  if (count > kMaxLines) count = kMaxLines;
  char text[kMaxLines][72];   // display bytes: SSIDs and URLs fit, the loop stack stays small
  TextStyle style[kMaxLines];
  int top[kMaxLines], height = 0;
  for (int i = 0; i < count; ++i) {
    gm::displayTextTo(text[i], sizeof(text[i]), lines[i].text ? lines[i].text : "");
    const int room = kWidth - 2 * kSafe - (lines[i].heading ? 14 : 0);
    style[i] = fit(text[i], room, lines[i].style);
    // Even Caption too wide: cut the end and mark it.
    if (width(style[i], text[i]) > room) {
      size_t len = strlen(text[i]);
      const int dots = width(style[i], "...");
      while (len && gm::styleTextWidth(style[i], text[i], len) + dots > room) --len;
      while (len && text[i][len - 1] == ' ') --len;
      memcpy(text[i] + len, "...", 4);
    }
    height += (i ? lines[i].gap : 0) + gm::styleLineHeight(style[i]);
  }
  int y0 = (kHeight - height) / 2;
  if (y0 < kSafe) y0 = kSafe;
  for (int i = 0, y = y0; i < count; ++i) {
    if (i) y += lines[i].gap;
    top[i] = y;
    y += gm::styleLineHeight(style[i]);
  }
  const Span span = {y0, y0 + height};
  uint16_t row[kWidth];
  for (int y = span.top; y < span.bottom && y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) row[x] = 0;
    for (int i = 0; i < count; ++i) {
      const int lh = gm::styleLineHeight(style[i]);
      // A glyph may reach a little past its own line, so neighbours are checked too.
      if (y < top[i] - lh / 4 || y >= top[i] + lh + lh / 4) continue;
      const int w = width(style[i], text[i]);
      if (lines[i].heading) {
        const int x = (kWidth - (w + 14)) / 2;
        drawDot(x + 4, top[i] + gm::styleBaseline(style[i]) - 5, y, accent, row);
        drawText(style[i], text[i], x + 14, top[i], y, lines[i].color, row);
      } else {
        drawText(style[i], text[i], (kWidth - w) / 2, top[i], y, lines[i].color, row);
      }
    }
    sink(context, y, row);
  }
  return span;
}

// A word of the device language (UTF-8, as every line's text: render() converts).
static const char* sysWord(sys::Word w, char* out, size_t cap) { return sys::text(w, out, cap); }

// The heading says what to do; the network, its password and the address
// follow in that order, so no further labels are needed (and with a failure
// reason and a password the block still fits the safe area).
int setupLines(const char* ssid, const char* pass, const char* ip, const char* why, Scratch& s, Line* out) {
  snprintf(s.text[0], sizeof(s.text[0]), "http://%s", ip ? ip : "");
  const bool open = !(pass && pass[0]);
  int n = 0;
  out[n++] = {sysWord(sys::WifiSetup, s.word[0], sizeof(s.word[0])), TextStyle::Caption, kHeading, 0, true};
  if (why && why[0]) out[n++] = {why, TextStyle::Caption, kBad, 4, false};
  out[n++] = {ssid ? ssid : "", TextStyle::Headline, kValue, 12, false};
  out[n++] = {sysWord(open ? sys::OpenNetwork : sys::Password, s.word[1], sizeof(s.word[1])), TextStyle::Caption, kHeading, 10, false};
  if (!open) out[n++] = {pass, TextStyle::Body, kValue, 0, false};
  out[n++] = {s.text[0], TextStyle::Body, kOk, 10, false};
  return n;
}

int connectedLines(const char* ssid, const char* ip, const char* host, Scratch& s, Line* out) {
  int n = 0;
  out[n++] = {sysWord(sys::Connected, s.word[0], sizeof(s.word[0])), TextStyle::Caption, kHeading, 0, true};
  out[n++] = {ssid && ssid[0] ? ssid : "-", TextStyle::Headline, kValue, 12, false};
  out[n++] = {sysWord(sys::OpenInBrowser, s.word[1], sizeof(s.word[1])), TextStyle::Caption, kHeading, 14, false};
  out[n++] = {ip && ip[0] ? ip : "-", TextStyle::Headline, kOk, 0, false};
  if (host && host[0]) {
    snprintf(s.text[1], sizeof(s.text[1]), "%s.local", host);
    out[n++] = {s.text[1], TextStyle::Caption, kHeading, 2, false};
  }
  return n;
}

int crashLines(const char* epc, const char* addr, const char* ip, Scratch& s, Line* out) {
  snprintf(s.text[0], sizeof(s.text[0]), "epc %s", epc && epc[0] ? epc : "-");
  snprintf(s.text[1], sizeof(s.text[1]), "addr %s", addr && addr[0] ? addr : "-");
  int n = 0;
  out[n++] = {sysWord(sys::Crashed, s.word[0], sizeof(s.word[0])), TextStyle::Caption, kHeading, 0, true};
  out[n++] = {s.text[0], TextStyle::Body, kValue, 14, false};
  out[n++] = {s.text[1], TextStyle::Body, kValue, 4, false};
  out[n++] = {sysWord(sys::FlashFixAt, s.word[1], sizeof(s.word[1])), TextStyle::Caption, kHeading, 10, false};
  out[n++] = {ip && ip[0] ? ip : "-", TextStyle::Body, kOk, 0, false};
  return n;
}

// ---- splash ------------------------------------------------------------------

void splashLayout(const char* detail, int pct, Splash& out) {
  gm::displayTextTo(out.detail, sizeof(out.detail), detail ? detail : "");
  out.detailStyle = TextStyle::Caption;
  const int room = kWidth - 2 * kSafe;
  if (width(out.detailStyle, out.detail) > room) {   // cut the end and mark it
    size_t len = strlen(out.detail);
    const int dots = width(out.detailStyle, "...");
    while (len && gm::styleTextWidth(out.detailStyle, out.detail, len) + dots > room) --len;
    while (len && out.detail[len - 1] == ' ') --len;
    memcpy(out.detail + len, "...", 4);
  }
  out.pct = pct > 100 ? 100 : pct;
  const int word = gm::styleLineHeight(TextStyle::Headline);
  // The wordmark sits just above the middle; the bar and the detail hang below.
  out.wordTop = kHeight / 2 - word + 4;
  out.barTop = out.wordTop + word + 16;
  out.barLeft = (kWidth - kBarW) / 2;
  out.detailTop = out.barTop + kBarH + 12;
  out.top = out.wordTop - 4;
  out.bottom = out.detailTop + gm::styleLineHeight(out.detailStyle) + 4;
}

void splashRow(const Splash& s, int y, uint16_t* row) {
  for (int x = 0; x < kWidth; ++x) row[x] = 0;
  const int word = gm::styleLineHeight(TextStyle::Headline);
  if (y >= s.wordTop - word / 4 && y < s.wordTop + word + word / 4) {
    static const char kWord[] = "GeekMagic";
    drawText(TextStyle::Headline, kWord, (kWidth - width(TextStyle::Headline, kWord)) / 2, s.wordTop, y, kValue, row);
  }
  if (s.pct >= 0 && y >= s.barTop && y < s.barTop + kBarH) {
    const int r = kBarH / 2, x0 = s.barLeft, x1 = s.barLeft + kBarW;
    int fill = kBarW * s.pct / 100;
    if (fill > 0 && fill < kBarH) fill = kBarH;   // a started bar shows as a dot, not a sliver
    for (int x = x0; x < x1; ++x) {
      const int track = roundRect(x, y, x0, s.barTop, x1, s.barTop + kBarH, r);
      uint16_t c = mix(0, kTrack, track);
      if (fill) c = mix(c, kValue, roundRect(x, y, x0, s.barTop, x0 + fill, s.barTop + kBarH, r));
      row[x] = c;
    }
  }
  if (s.detail[0]) {
    const int lh = gm::styleLineHeight(s.detailStyle);
    if (y >= s.detailTop - lh / 4 && y < s.detailTop + lh + lh / 4)
      drawText(s.detailStyle, s.detail, (kWidth - width(s.detailStyle, s.detail)) / 2, s.detailTop, y, kDim, row);
  }
}

// ---- the setup QR page -------------------------------------------------------------

void qrLayout(const qr::Code& code, QrPage& out) {
  out.code = &code;
  // Modules as large as fit a 132 px card with the two-module quiet zone
  // (a version 3 code, the usual hotspot, gets 4 px modules).
  out.module = code.size ? 132 / (code.size + 4) : 1;
  if (out.module > 4) out.module = 4;
  if (out.module < 1) out.module = 1;
  out.card = (code.size + 4) * out.module;
  const int head = gm::styleLineHeight(TextStyle::Caption), gap = 12;
  const int block = head + gap + out.card + gap + head;
  out.top = (kHeight - block) / 2;
  if (out.top < kSafe) out.top = kSafe;
  out.headTop = out.top;
  out.cardTop = out.top + head + gap;
  out.cardLeft = (kWidth - out.card) / 2;
  out.capTop = out.cardTop + out.card + gap;
  out.bottom = out.capTop + head;
}

Span renderQr(const QrPage& p, const char* heading, const char* caption, uint16_t accent, RowSink sink, void* context) {
  char head[48], cap[48];
  gm::displayTextTo(head, sizeof(head), heading ? heading : "");
  gm::displayTextTo(cap, sizeof(cap), caption ? caption : "");
  const TextStyle st = TextStyle::Caption;
  const int lh = gm::styleLineHeight(st), hw = width(st, head), cw = width(st, cap);
  const int radius = 3 * p.module < 12 ? 12 : 3 * p.module;
  uint16_t row[kWidth];
  for (int y = p.top - lh / 4; y < p.bottom + lh / 4; ++y) {
    if (y < 0 || y >= kHeight) continue;
    for (int x = 0; x < kWidth; ++x) row[x] = 0;
    if (y < p.headTop + lh + lh / 4) {
      const int x = (kWidth - (hw + 14)) / 2;
      drawDot(x + 4, p.headTop + gm::styleBaseline(st) - 5, y, accent, row);
      drawText(st, head, x + 14, p.headTop, y, kHeading, row);
    }
    if (y >= p.cardTop && y < p.cardTop + p.card) {
      const int my = (y - p.cardTop) / p.module - 2;
      for (int x = p.cardLeft; x < p.cardLeft + p.card; ++x) {
        const int a = roundRect(x, y, p.cardLeft, p.cardTop, p.cardLeft + p.card, p.cardTop + p.card, radius);
        if (!a) continue;
        const int mx = (x - p.cardLeft) / p.module - 2;
        const bool dark = mx >= 0 && my >= 0 && mx < p.code->size && my < p.code->size && p.code->dark(mx, my);
        row[x] = dark ? 0 : mix(0, 0xFFFF, a);
      }
    }
    if (y >= p.capTop - lh / 4) drawText(st, cap, (kWidth - cw) / 2, p.capTop, y, kHeading, row);
    sink(context, y, row);
  }
  return {p.top, p.bottom};
}

}  // namespace screenview
