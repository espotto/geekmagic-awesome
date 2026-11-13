// NotifyView.h — the notification screen's layout and text rendering, kept free
// of Arduino and the display driver so the desktop can test and preview it.
//
// The look is an iOS banner: a dark grey card with rounded corners, a small
// icon in the accent colour with the title's first letter, the title in grey
// beside it, and the label in white under them, left-aligned. The label is
// wrapped at word boundaries: up to two lines in the large face, three in the
// medium one, otherwise five in the small one, the last cut with "..." if
// even that is not enough. A short line centred under the card (NotifyMode) shows the time
// left. Everything stays inside the safe area the device's frame leaves.
//
// Text is in display bytes (gm::displayText): ASCII, Cyrillic, °, drawn
// in the system font (SystemFont.h).
#pragma once
#include <stdint.h>

namespace notifyview {

constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kCardX = 20;            // the card: x from kCardX, kCardW wide
constexpr int kCardW = 200;
constexpr int kCardRadius = 18;
constexpr int kPad = 14;              // inside the card
constexpr int kIcon = 22;             // the accent icon, square
constexpr int kTextW = kCardW - 2 * kPad;   // the label's width
constexpr uint16_t kCardColor = 0x2965;     // #2c2c2e
constexpr int kProgressW = 64;        // the time-left line under the text
constexpr int kProgressH = 3;
constexpr int kProgressGap = 14;      // between the last text row and the line
constexpr uint16_t kTrackColor = 0x2965;   // #2c2c2e, what the line leaves behind
constexpr int kMaxLines = 5;
constexpr uint16_t kTitleColor = 0x8C72;   // #8e8e93
constexpr uint16_t kLabelColor = 0xF7BE;   // #f5f5f7

struct Layout {
  const char* title = "";
  const char* label = "";
  uint8_t size = 2;                   // label face: 2 large, 1 medium, 0 small
  uint8_t lines = 0;
  uint8_t start[kMaxLines] = {0}, len[kMaxLines] = {0};
  int16_t width[kMaxLines] = {0};     // including the "..." on a cut last line
  bool ellipsis = false;
  int16_t top = 0;                    // first row of the card
  int16_t labelTop = 0;               // first row of the label lines
  int16_t bottom = 0;                 // one past the card's last row
  int16_t progressX = 0, progressY = 0;   // the time-left line, kProgressW x kProgressH
};

// Lays out `title` and `label` (NUL-terminated display bytes, which must
// outlive the layout). With no label the title is shown as the label, beside
// a plain icon.
void layout(const char* title, const char* label, Layout& out);

// Composes screen row `y` of the card into `row` (kWidth RGB565 pixels, black
// around it). False for a row outside the card.
bool renderRow(const Layout& l, uint16_t accent, int y, uint16_t* row);

// Advance width of `n` display bytes in the label face of `size` or,
// with title true, the title face. Exposed for tests.
int textWidth(const char* s, int n, int size, bool title = false);

}  // namespace notifyview
