// The Timer app's ring screen and the night clock on the desktop: the ring
// runs down with the time, the arc ends where the time says, everything stays
// inside the safe area; with a path argument sample screens go to a PPM.
#include "timer/TimerView.h"
#include "nightstand/NightstandView.h"
#include "ppm.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
using namespace timerview;

static std::vector<uint16_t> frame(const Layout& l) {
  std::vector<uint16_t> px(240 * 240);
  for (int y = 0; y < 240; ++y) renderRow(l, y, &px[y * 240]);
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 240; ++x)
      if (px[y * 240 + x]) assert(x >= 19 && x < 221 && y >= 19 && y < 221);
  return px;
}
static bool orangeAt(const std::vector<uint16_t>& px, int deg) {
  // A point on the ring's middle at `deg` clockwise from 12 o'clock.
  const double a = deg * 3.14159265 / 180;
  const int x = 120 + (int)(92 * sin(a)), y = 120 - (int)(92 * cos(a));
  return px[y * 240 + x] == 0xFCE1;
}

int main(int argc, char** argv) {
  Layout l;
  std::vector<std::vector<uint16_t>> frames;
  layout(State::Running, 225000, 300000, "", 14, 35, 1, l);   // 3:45 of 5:00 left: 270 degrees
  assert(l.sweep == 270 && std::strcmp(l.digits, "03:45") == 0);
  auto px = frame(l);
  assert(orangeAt(px, 45) && orangeAt(px, 200) && orangeAt(px, 260) && !orangeAt(px, 300));
  frames.push_back(px);
  layout(State::Running, 40000, 300000, "Чай", 14, 35, 1, l);
  px = frame(l);
  assert(orangeAt(px, 20) && !orangeAt(px, 90));
  frames.push_back(px);
  layout(State::Paused, 4000000, 5400000, "", -1, 0, 0, l);   // over an hour
  assert(std::strcmp(l.digits, "1:06:40") == 0);
  frames.push_back(frame(l));
  layout(State::Idle, 300000, 300000, "", -1, 0, 1, l);
  frames.push_back(frame(l));
  // A second shows until it has fully passed.
  layout(State::Running, 59001, 60000, "", -1, 0, 0, l);
  assert(std::strcmp(l.digits, "01:00") == 0);
  char t[32], b[32];
  bannerWords(1, t, sizeof(t), b, sizeof(b));
  assert(std::strcmp(t, "Таймер") == 0 && std::strcmp(b, "Время вышло") == 0);

  nightstandview::Layout n;
  nightstandview::layout(2, 47, 5, 13, 11, 1, n);
  std::vector<uint16_t> np(240 * 240);
  for (int y = 0; y < 240; ++y) nightstandview::renderRow(n, y, &np[y * 240]);
  frames.push_back(np);
  if (argc > 1) writeFrames(argv[1], frames);
  std::cout << "timer and nightstand view tests passed\n";
}
