// PomodoroView.h — the Pomodoro app's screen: the Timer's ring (TimerView) in
// the colour of the phase (red focus, green short break, blue long break), the
// phase's name above the time, when it ends below, and a dot per focus session
// of the set. Without Arduino (tests/apps).
#pragma once
#include <stdint.h>
#include "PomodoroCore.h"
#include "../timer/TimerView.h"

namespace pomoview {

// `endHour` < 0 when the clock is not set.
void layout(const pomo::Core& c, const pomo::Plan& p, uint32_t now, int endHour, int endMin,
            uint8_t lang, timerview::Layout& out);

// The banner when `ended` runs out: "Pomodoro" and "Time for a break" or
// "Time to focus", in `lang`.
void bannerWords(uint8_t lang, pomo::Phase ended, char* title, int titleCap, char* text, int textCap);
// The phase's name in `lang` (UTF-8, for the banner of a label).
void phaseName(uint8_t lang, pomo::Phase p, char* out, int cap);

}  // namespace pomoview
