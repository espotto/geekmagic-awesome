// TimerView.h — the Timer app's screen, after a phone's Clock app: an orange
// ring that runs down as the time does, the time left large inside it, the
// timer's name above and the time it ends below ("Paused" while paused).
// Rendered a row at a time, without Arduino (tests/apps).
#pragma once
#include <stdint.h>

namespace timerview {

enum class State : uint8_t { Idle, Running, Paused };

struct Layout {
  State state = State::Idle;
  int sweep = 0;               // degrees of ring still orange, 0-360
  int endX = 0, endY = 0;      // the arc's end, 1/16 px
  int ex = 0, ey = 0;          // its direction, scaled by 1024 (x right, y down)
  char digits[12] = {0};       // display bytes
  bool smallDigits = false;    // over an hour: Headline instead of DisplayS
  char label[28] = {0};
  char sub[28] = {0};
};

// `leftMs` of `totalMs`; `endHour`/`endMin` < 0 when the clock is not set.
void layout(State st, uint32_t leftMs, uint32_t totalMs, const char* label,
            int endHour, int endMin, uint8_t lang, Layout& out);
void renderRow(const Layout& l, int y, uint16_t* row);

// The words of the end-of-timer banner: "Timer" and "Time's up", in `lang`.
void bannerWords(uint8_t lang, char* title, int titleCap, char* text, int textCap);

}  // namespace timerview
