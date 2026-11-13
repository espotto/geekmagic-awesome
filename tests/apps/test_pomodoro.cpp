// The Pomodoro app: its clock (PomodoroCore) runs focus sessions, short
// breaks and a long break after every `rounds`, with pause, skip and reset;
// its screen (PomodoroView) is the Timer's ring in the colour of the phase,
// with a dot per session, inside the safe area. With a path argument sample
// screens go to a PPM.
#include "pomodoro/PomodoroView.h"
#include "ppm.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace pomo;

static const uint32_t MIN = 60000;

static std::vector<uint16_t> frame(const timerview::Layout& l) {
  std::vector<uint16_t> px(240 * 240);
  for (int y = 0; y < 240; ++y) timerview::renderRow(l, y, &px[y * 240]);
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 240; ++x)
      if (px[y * 240 + x]) assert(x >= 19 && x < 221 && y >= 19 && y < 221);
  return px;
}

static void testCycle() {
  Plan p;   // 25 / 5 / 15, four rounds, the next phase starts by itself
  Core c;
  Phase ended;
  assert(c.run() == Run::Idle && c.phase() == Phase::Focus && c.leftMs(0, p) == 25 * MIN);
  uint32_t t = 1000;
  c.start(t, p);
  assert(c.run() == Run::Running && c.leftMs(t + 10 * MIN, p) == 15 * MIN);
  assert(!c.tick(t + 25 * MIN - 1, p, ended));
  // Four focus sessions with a short break after the first three, then the long one.
  for (int round = 1; round <= 4; ++round) {
    t += 25 * MIN;
    assert(c.tick(t, p, ended) && ended == Phase::Focus && c.done() == round);
    if (round < 4) {
      assert(c.phase() == Phase::Short && c.run() == Run::Running && c.totalMs(p) == 5 * MIN);
      t += 5 * MIN;
      assert(c.tick(t, p, ended) && ended == Phase::Short && c.phase() == Phase::Focus);
    }
  }
  assert(c.phase() == Phase::Long && c.totalMs(p) == 15 * MIN && c.done() == 4);
  t += 15 * MIN;
  assert(c.tick(t, p, ended) && ended == Phase::Long && c.phase() == Phase::Focus && c.done() == 0);
  // A late pass does not stretch the cycle: the next phase started on time.
  t += 25 * MIN;
  assert(c.tick(t + 2000, p, ended) && c.leftMs(t + 2000, p) == 5 * MIN - 2000);
}

static void testManual() {
  Plan p;
  p.autoNext = false;
  p.rounds = 2;
  p.focusMin = 1;
  Core c;
  Phase ended;
  c.start(0, p);
  c.pause(20000);
  assert(c.run() == Run::Paused && c.leftMs(999999, p) == 40000);
  assert(!c.tick(999999, p, ended));
  c.start(100000, p);   // resume
  assert(c.run() == Run::Running && c.leftMs(100000, p) == 40000);
  assert(c.tick(140000, p, ended) && ended == Phase::Focus);
  // Without autoNext the break waits to be started.
  assert(c.phase() == Phase::Short && c.run() == Run::Idle && c.leftMs(500000, p) == 5 * MIN);
  // Skip while stopped: on to the next phase, still stopped.
  c.skip(150000, p);
  assert(c.phase() == Phase::Focus && c.run() == Run::Idle);
  // Skip while running: the next one runs at once; two sessions make a set.
  c.start(200000, p);
  c.skip(210000, p);
  assert(c.phase() == Phase::Long && c.done() == 2 && c.run() == Run::Running && c.leftMs(210000, p) == 15 * MIN);
  c.reset();
  assert(c.phase() == Phase::Focus && c.run() == Run::Idle && c.done() == 0);
  // The length a phase started with stays when the settings change.
  c.start(0, p);
  p.focusMin = 50;
  assert(c.totalMs(p) == 1 * MIN && c.leftMs(30000, p) == 30000);
}

int main(int argc, char** argv) {
  testCycle();
  testManual();

  std::vector<std::vector<uint16_t>> frames;
  Plan p;
  Core c;
  timerview::Layout l;
  pomoview::layout(c, p, 0, -1, 0, 1, l);
  assert(std::strcmp(l.digits, "25:00") == 0 && l.dots == 4 && l.dotsOn == 0);
  frames.push_back(frame(l));
  c.start(0, p);
  Phase ended;
  c.tick(25 * MIN, p, ended);   // one session done, in the short break
  pomoview::layout(c, p, 26 * MIN, 14, 35, 1, l);
  assert(l.arc == 0x368B && l.dotsOn == 1 && std::strcmp(l.digits, "04:00") == 0);
  auto px = frame(l);
  assert(px[(120 - 92) * 240 + 120] == 0x368B);   // the ring at 12 o'clock is green
  frames.push_back(px);
  c.skip(26 * MIN, p);
  c.pause(30 * MIN);
  pomoview::layout(c, p, 30 * MIN, -1, 0, 0, l);
  assert(l.state == timerview::State::Paused && l.arc == 0xFA27);
  frames.push_back(frame(l));
  for (int i = 0; i < 5; ++i) c.skip(0, p);   // sessions 2-4 and their breaks: into the long break
  pomoview::layout(c, p, 0, -1, 0, 1, l);
  assert(c.phase() == Phase::Long && l.dotsOn == 4 && l.arc == 0x0C3F);
  frames.push_back(frame(l));

  char title[40], text[40], name[40];
  pomoview::bannerWords(1, Phase::Focus, title, sizeof(title), text, sizeof(text));
  assert(std::strcmp(title, "Помодоро") == 0 && std::strcmp(text, "Пора отдохнуть") == 0);
  pomoview::bannerWords(0, Phase::Long, title, sizeof(title), text, sizeof(text));
  assert(std::strcmp(text, "Time to focus") == 0);
  pomoview::phaseName(1, Phase::Long, name, sizeof(name));
  assert(std::strcmp(name, "Длинный перерыв") == 0);
  if (argc > 1) writeFrames(argv[1], frames);
  std::cout << "pomodoro tests passed\n";
}
