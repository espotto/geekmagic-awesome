#include "PomodoroView.h"
#include "../AppDraw.h"

namespace pomoview {
namespace {

// Per language (en ru): the three phases, then the banner's title and its two texts.
const char kWords[] PROGMEM =
  "Focus|Short Break|Long Break|Pomodoro|Time for a break|Time to focus|"
  "Фокус|Перерыв|Длинный перерыв|Помодоро|Пора отдохнуть|Пора за работу";
constexpr int kPerLang = 6;

struct Colours { uint16_t on, dim; };
// #ff453a, #30d158, #0a84ff and each at about half.
Colours colours(pomo::Phase p) {
  switch (p) {
    case pomo::Phase::Short: return {0x368B, 0x1B45};
    case pomo::Phase::Long:  return {0x0C3F, 0x020F};
    default:                 return {0xFA27, 0x7903};
  }
}

}  // namespace

void layout(const pomo::Core& c, const pomo::Plan& p, uint32_t now, int endHour, int endMin,
            uint8_t lang, timerview::Layout& out) {
  if (lang > 1) lang = 0;
  const timerview::State st = c.run() == pomo::Run::Running ? timerview::State::Running
                            : c.run() == pomo::Run::Paused ? timerview::State::Paused
                                                           : timerview::State::Idle;
  char name[40];
  phaseName(lang, c.phase(), name, sizeof(name));
  timerview::layout(st, c.leftMs(now, p), c.totalMs(p), name, endHour, endMin, lang, out);
  const Colours k = colours(c.phase());
  out.arc = k.on;
  out.arcDim = k.dim;
  out.labelColour = k.on;
  out.dots = p.rounds > 8 ? 8 : p.rounds;
  out.dotsOn = c.phase() == pomo::Phase::Long ? out.dots : (c.done() > out.dots ? out.dots : c.done());
}

void phaseName(uint8_t lang, pomo::Phase p, char* out, int cap) {
  if (lang > 1) lang = 0;
  appdraw::entry(kWords, lang * kPerLang + (int)p, out, cap);
}

void bannerWords(uint8_t lang, pomo::Phase ended, char* title, int titleCap, char* text, int textCap) {
  if (lang > 1) lang = 0;
  appdraw::entry(kWords, lang * kPerLang + 3, title, titleCap);
  appdraw::entry(kWords, lang * kPerLang + (ended == pomo::Phase::Focus ? 4 : 5), text, textCap);
}

}  // namespace pomoview
