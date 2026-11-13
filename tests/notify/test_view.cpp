// The notification screen's layout and rendering (NotifyView), on the desktop.
// With an output path argument it also writes the sample screens side by side
// as a binary PPM, for a visual check:
//   tests/notify/run.sh /tmp/notify.ppm
#include "NotifyView.h"
#include "ThemeEngine.h"   // displayText: UTF-8 to display bytes
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace notifyview;

struct Shot {
  std::string title, label;
  uint16_t accent;
  double progress;
};

static void frame(const Shot& s, std::vector<uint16_t>& px) {
  px.assign(kWidth * kHeight, 0);
  const std::string title = gm::displayText(s.title), label = gm::displayText(s.label);
  Layout l;
  layout(title.c_str(), label.c_str(), l);
  uint16_t row[kWidth];
  for (int y = 0; y < kHeight; ++y)
    if (renderRow(l, s.accent, y, row)) std::memcpy(&px[y * kWidth], row, sizeof(row));
  const int filled = static_cast<int>(kProgressW * s.progress);
  for (int y = l.progressY; y < l.progressY + kProgressH; ++y)
    for (int x = 0; x < kProgressW; ++x) px[y * kWidth + l.progressX + x] = x < filled ? s.accent : kTrackColor;
}

static Layout laid(const std::string& title, const std::string& label, std::string& t, std::string& lb) {
  t = gm::displayText(title);
  lb = gm::displayText(label);
  Layout l;
  layout(t.c_str(), lb.c_str(), l);
  return l;
}

int main(int argc, char** argv) {
  std::string t, lb;
  // Short: one line in the large face, inside the card.
  Layout l = laid("Погода", "На улице -5°", t, lb);
  assert(l.size == 2 && l.lines == 1 && !l.ellipsis);
  assert(l.width[0] > 0 && l.width[0] <= kTextW);
  assert(l.top > 0 && l.labelTop > l.top);
  assert(l.progressY == l.bottom + kProgressGap && l.progressX == (kWidth - kProgressW) / 2);
  // Two lines still fit the large face.
  l = laid("Дом", "Кофе готов, пора пить", t, lb);
  assert(l.size == 2 && l.lines == 2);
  // Three large lines do not: the medium face takes it.
  l = laid("Внимание", "Диск NAS заполнен на 95%", t, lb);
  assert(l.size == 1 && l.lines >= 2 && l.lines <= 3);
  // Longer: the small face, up to five lines, words kept whole.
  l = laid("Сервер", "Резервная копия завершена успешно за 3 минуты", t, lb);
  assert(l.size == 0 && l.lines >= 3 && l.lines <= kMaxLines && !l.ellipsis);
  for (int i = 0; i < l.lines; ++i) {
    assert(l.width[i] <= kTextW);
    assert(lb[l.start[i]] != ' ' && lb[l.start[i] + l.len[i] - 1] != ' ');
  }
  // Even five small lines keep the card and the time-left line inside the safe area.
  assert(l.top >= 20 && l.progressY + kProgressH <= kHeight - 20);
  // Too long even for five small lines: the last one is cut with "...".
  l = laid("Лог", "Очень длинное сообщение, которое никак не помещается в четыре строки на этом маленьком экране часов", t, lb);
  assert(l.lines == kMaxLines && l.ellipsis && l.width[kMaxLines - 1] <= kTextW);
  // A single word wider than a line is broken between letters.
  l = laid("", "Ааааааааааааааааааааааааааааааааааааааааа", t, lb);
  assert(l.lines >= 2);
  for (int i = 0; i < l.lines; ++i) assert(l.width[i] <= kTextW);
  // No label: the title becomes the label, beside a plain icon.
  l = laid("TASK DONE", "", t, lb);
  assert(l.lines == 1 && *l.title == 0 && std::string(l.label) == "TASK DONE");
  // Nothing at all: the card with its icon.
  l = laid("", "", t, lb);
  assert(l.lines == 0 && l.bottom > l.top);
  // Rows outside the card are left alone; inside, the card stays within its
  // columns, its corners are rounded, and the text inks it.
  {
    l = laid("Погода", "На улице -5°", t, lb);
    uint16_t row[kWidth];
    assert(!renderRow(l, 0xFEA1, l.top - 1, row) && !renderRow(l, 0xFEA1, l.bottom, row));
    bool ink = false;
    for (int y = l.top; y < l.bottom; ++y) {
      assert(renderRow(l, 0xFEA1, y, row));
      for (int x = 0; x < kWidth; ++x) {
        if (x < kCardX || x >= kCardX + kCardW) assert(row[x] == 0);
        if (y >= l.labelTop && row[x] != kCardColor && row[x] != 0) ink = true;
      }
    }
    assert(ink);
    assert(renderRow(l, 0xFEA1, l.top, row) && row[kCardX] == 0 && row[kWidth / 2] == kCardColor);
  }
  // Every display byte has a glyph or falls back to the space.
  for (int c = 32; c < 256; ++c) {
    const char s[2] = {static_cast<char>(c), 0};
    assert(textWidth(s, 1, 2) > 0 && textWidth(s, 1, 0, true) > 0);
  }

  if (argc > 1) {
    const Shot shots[] = {
      {"Погода", "На улице -5°", 0xFEA1, 0.7},
      {"Сервер", "Резервная копия завершена успешно за 3 минуты", 0x2E8B, 0.4},
      {"Внимание", "Диск NAS заполнен на 95%", 0xFA27, 0.9},
      {"Дом", "Кофе готов, пора пить", 0x5D7F, 0.6},
      {"TASK DONE", "", 0x2E8B, 0.2},
      {"Лог", "Очень длинное сообщение, которое никак не помещается в четыре строки на этом маленьком экране часов", 0x5D7F, 0.5},
    };
    const int n = sizeof(shots) / sizeof(shots[0]), gap = 12, W = n * kWidth + (n - 1) * gap;
    std::vector<unsigned char> out(W * kHeight * 3, 60);
    std::vector<uint16_t> px;
    for (int i = 0; i < n; ++i) {
      frame(shots[i], px);
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
  std::cout << "notify view tests passed\n";
}
