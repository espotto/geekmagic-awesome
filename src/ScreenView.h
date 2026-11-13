// ScreenView.h — the firmware's own screens (boot, Wi-Fi setup, connected,
// crash), laid out and rendered without Arduino or the display driver, so the
// desktop can test and preview them (tests/screens). Gfx.cpp sends the rows.
//
// The look is the notifications': black, a small accent dot beside a grey
// heading, grey labels over white values, the block centred and kept at least
// kSafe px from the edges, which the device's case covers. Text is UTF-8 and
// drawn in the system font (features/theme/SystemFont.h); a value too wide for
// its style steps down to a smaller one.
#pragma once
#include <stdint.h>
#include "features/theme/SystemFont.h"
#include "QrCode.h"

namespace screenview {

constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kSafe = 20;
constexpr int kMaxLines = 8;
constexpr uint16_t kHeading = 0x8C72;   // #8e8e93
constexpr uint16_t kValue   = 0xF7BE;   // #f5f5f7
constexpr uint16_t kOk      = 0x2E8B;   // #30d158
constexpr uint16_t kWarn    = 0xFEA1;   // #ffd60a
constexpr uint16_t kBad     = 0xFA27;   // #ff453a

// One line: UTF-8 text, the largest style it may use, its colour, the gap above
// it, and whether it is the heading (drawn with the accent dot to its left).
struct Line {
  const char* text;
  gm::TextStyle style;
  uint16_t color;
  uint8_t gap;
  bool heading;
};

// Receives one finished screen row of kWidth RGB565 pixels (untinted).
typedef void (*RowSink)(void* context, int y, const uint16_t* row);

// Lays out `lines` and hands every row of the text block to `sink`; the rest
// of the screen is black and left to the caller. Returns the rows used.
struct Span { int top, bottom; };
Span render(const Line* lines, int count, uint16_t accent, RowSink sink, void* context);

// The screens themselves, as the lines they show. Each fills `out` and returns
// the line count; `scratch` keeps strings built here (the URL) alive.
// Room for the lines' own text: formatted values and the words in the device
// language (SysText), UTF-8.
struct Scratch { char text[2][80]; char word[2][48]; };
int setupLines(const char* ssid, const char* pass, const char* ip, const char* why, Scratch& s, Line* out);
int connectedLines(const char* ssid, const char* ip, const char* host, Scratch& s, Line* out);
int crashLines(const char* epc, const char* addr, const char* ip, Scratch& s, Line* out);

// ---- the splash: start-up, updates ------------------------------------------
// The wordmark in the middle, a thin rounded bar under it and a grey detail
// line under that, the way a Mac starts. `pct` is 0-100, or -1 for no bar.
constexpr int kBarW = 96;
constexpr int kBarH = 4;
constexpr uint16_t kTrack = 0x39C7;   // #3a3a3c, the bar's empty part
constexpr uint16_t kDim   = 0x632C;   // #636366, the detail line

struct Splash {
  char detail[48];
  gm::TextStyle detailStyle;
  int pct;
  int wordTop, barTop, barLeft, detailTop, top, bottom;   // [top, bottom): rows it uses
};
void splashLayout(const char* detail, int pct, Splash& out);
// Row `y` of the splash (any row; black where it shows nothing).
void splashRow(const Splash& s, int y, uint16_t* row);

// ---- the Wi-Fi setup QR page ------------------------------------------------
// The heading with its dot, the code dark on a white rounded card, and a
// caption under it. The card keeps a quiet zone of two modules.
struct QrPage {
  const qr::Code* code;
  int module, card, cardLeft, cardTop, headTop, capTop, top, bottom;
};
void qrLayout(const qr::Code& code, QrPage& out);
Span renderQr(const QrPage& p, const char* heading, const char* caption, uint16_t accent, RowSink sink, void* context);

}  // namespace screenview
