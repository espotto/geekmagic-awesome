// The Reminders app on the desktop: the list's rules, and the screen staying
// inside the safe area with open items first and an overflow line. With an
// output path argument the sample screens are written side by side as a PPM:
//   tests/reminders/run.sh /tmp/reminders.ppm
#include "RemindersList.h"
#include "RemindersView.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace reminders;
using namespace remindersview;

static std::vector<uint16_t> frame(const Layout& l) {
  std::vector<uint16_t> px(kWidth * kHeight);
  for (int y = 0; y < kHeight; ++y) renderRow(l, y, &px[y * kWidth]);
  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x)
      if (px[y * kWidth + x]) assert(x >= kSafe - 1 && x < kWidth - kSafe + 1 && y >= kSafe - 1 && y < kHeight - kSafe + 1);
  return px;
}

int main(int argc, char** argv) {
  List list;
  // Cleaning: trimmed, controls to spaces, empty refused, cut on a character boundary.
  char t[kTextMax + 1];
  assert(List::clean("  Купить\tхлеб \n", t) && std::string(t) == "Купить хлеб");
  assert(!List::clean("   ", t) && !List::clean(nullptr, t));
  const std::string longRu(80, 'x');
  std::string cyr;
  for (int i = 0; i < 40; ++i) cyr += "ж";   // 80 bytes
  assert(List::clean(cyr.c_str(), t) && strlen(t) == 62 && (strlen(t) % 2) == 0);
  assert(List::clean(longRu.c_str(), t) && strlen(t) == kTextMax);
  // Add, done, remove, clear.
  assert(list.add("Молоко") && list.add("Позвонить маме") && list.add("Оплатить интернет"));
  assert(list.count() == 3 && list.open() == 3);
  assert(list.setDone(0, true) && list.open() == 2 && !list.setDone(5, true));
  assert(list.remove(1) && list.count() == 2 && std::string(list.at(1).text) == "Оплатить интернет");
  assert(list.clearDone() == 1 && list.count() == 1 && list.open() == 1);
  // A full list drops its oldest done item to make room; with none done it refuses.
  list.clear();
  for (int i = 0; i < kMax; ++i) assert(list.add(("item " + std::to_string(i)).c_str()));
  assert(!list.add("one more"));
  list.setDone(3, true);
  assert(list.add("one more") && list.count() == kMax && std::string(list.at(3).text) == "item 4");

  // The screen: open items first, overflow line, empty list.
  Layout l;
  list.clear();
  layout(list, 1, l);
  assert(l.shown == 0 && l.more == 0 && std::string(l.count) == "0");
  frame(l);
  for (const char* s : {"Купить молоко", "Позвонить маме", "Забрать посылку на почте до пятницы", "Полить цветы"}) list.add(s);
  list.setDone(0, true);
  layout(list, 1, l);
  assert(l.shown == 4 && l.more == 0 && l.order[0] == 1 && l.order[3] == 0 && l.done[3] && std::string(l.count) == "3");
  frame(l);
  for (int i = 0; i < 4; ++i) list.add(("Дело " + std::to_string(i)).c_str());
  layout(list, 1, l);
  assert(l.more > 0 && l.shown + l.more == list.count());
  frame(l);
  // Every language's title fits beside the count.
  for (uint8_t lang = 0; lang < 2; ++lang) { layout(list, lang, l); frame(l); }

  if (argc > 1) {
    List a, b, c;
    for (const char* s : {"Купить молоко", "Позвонить маме", "Забрать посылку на почте до пятницы", "Полить цветы"}) a.add(s);
    a.setDone(0, true);
    for (const char* s : {"Купить молоко", "Позвонить маме", "Забрать посылку", "Полить цветы", "Оплатить интернет", "Записаться к врачу", "Вынести мусор"}) b.add(s);
    const List* lists[] = {&a, &b, &c};
    const int n = 3, gap = 12, W = n * kWidth + (n - 1) * gap;
    std::vector<unsigned char> out(W * kHeight * 3, 60);
    for (int i = 0; i < n; ++i) {
      layout(*lists[i], 1, l);
      const std::vector<uint16_t> px = frame(l);
      for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x) {
          const uint16_t p = px[y * kWidth + x];
          unsigned char* o = &out[(y * W + i * (kWidth + gap) + x) * 3];
          o[0] = ((p >> 11) * 255) / 31;
          o[1] = (((p >> 5) & 63) * 255) / 63;
          o[2] = ((p & 31) * 255) / 31;
        }
    }
    FILE* f = std::fopen(argv[1], "wb");
    assert(f);
    std::fprintf(f, "P6\n%d %d\n255\n", W, kHeight);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
  }
  std::cout << "reminders tests passed\n";
}
