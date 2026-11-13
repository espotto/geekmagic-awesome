// The Calendar app's month screen (CalendarView), on the desktop: the date
// arithmetic, the grid, today's circle, the safe area; with an output path
// argument the sample months are written side by side as a PPM:
//   tests/calendar/run.sh /tmp/calendar.ppm
#include "CalendarView.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
using namespace calendarview;

static std::vector<uint16_t> frame(const Layout& l) {
  std::vector<uint16_t> px(kWidth * kHeight);
  for (int y = 0; y < kHeight; ++y) renderRow(l, y, &px[y * kWidth]);
  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x)
      if (px[y * kWidth + x]) assert(x >= kSafe - 1 && x < kWidth - kSafe + 1 && y >= kSafe - 1 && y < kHeight - kSafe + 1);
  return px;
}

int main(int argc, char** argv) {
  // Date arithmetic.
  assert(weekday(2026, 11, 12) == 4 && weekday(2000, 1, 1) == 6 && weekday(2024, 2, 29) == 4);
  assert(daysIn(2024, 2) == 29 && daysIn(2026, 2) == 28 && daysIn(1900, 2) == 28 && daysIn(2000, 2) == 29 && daysIn(2026, 11) == 30);
  // November 2026 starts on a Sunday: the last column in Russian, the first in English.
  Layout l;
  layout(2026, 11, 12, 1, l);
  assert(l.firstCol == 6 && l.rows == 6 && l.days == 30);
  layout(2026, 11, 12, 0, l);
  assert(l.firstCol == 0 && l.rows == 5);
  // February 2027 starts on a Monday and fills exactly four weeks.
  layout(2027, 2, 0, 1, l);
  assert(l.firstCol == 0 && l.rows == 4 && l.today == 0);
  // Every month of a few years fits the safe area, and today is on a red circle.
  for (int y = 2025; y <= 2028; ++y)
    for (int m = 1; m <= 12; ++m)
      for (uint8_t lang : {0, 6}) {
        layout(y, m, 15, lang, l);
        assert(l.top >= kSafe && l.bottom <= kHeight - kSafe);
        const std::vector<uint16_t> px = frame(l);
        int cx, cy;
        cellCentre(l, 15, cx, cy);
        assert(px[(cy - 9) * kWidth + cx] == kRed);
        cellCentre(l, 16, cx, cy);
        assert(px[(cy - 9) * kWidth + cx] == 0);
      }

  // The longest month names stay clear of the year.
  for (uint8_t lang = 0; lang < 2; ++lang)
    for (int m = 1; m <= 12; ++m) {
      layout(2026, m, 0, lang, l);
      const std::vector<uint16_t> px = frame(l);
      int lastRed = 0, firstGrey = kWidth;
      for (int y = l.headTop; y < l.initialsTop; ++y)
        for (int x = 0; x < kWidth; ++x) {
          const uint16_t c = px[y * kWidth + x];
          if (c == kRed && x > lastRed) lastRed = x;
          if (c == kGrey && x < firstGrey) firstGrey = x;
        }
      assert(lastRed + 6 < firstGrey);
    }

  if (argc > 1) {
    struct { int y, m, d; uint8_t lang; } shots[] = {{2026, 11, 12, 6}, {2027, 2, 14, 6}, {2026, 9, 30, 6}, {2026, 11, 12, 0}};
    const int n = 4, gap = 12, W = n * kWidth + (n - 1) * gap;
    std::vector<unsigned char> out(W * kHeight * 3, 60);
    for (int i = 0; i < n; ++i) {
      layout(shots[i].y, shots[i].m, shots[i].d, shots[i].lang, l);
      const std::vector<uint16_t> px = frame(l);
      for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x) {
          const uint16_t c = px[y * kWidth + x];
          unsigned char* o = &out[(y * W + i * (kWidth + gap) + x) * 3];
          o[0] = ((c >> 11) * 255) / 31;
          o[1] = (((c >> 5) & 63) * 255) / 63;
          o[2] = ((c & 31) * 255) / 31;
        }
    }
    FILE* f = std::fopen(argv[1], "wb");
    assert(f);
    std::fprintf(f, "P6\n%d %d\n255\n", W, kHeight);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
  }
  std::cout << "calendar view tests passed\n";
}
