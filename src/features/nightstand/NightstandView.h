// NightstandView.h — the night clock: at night (Display -> Night), over any
// app, the time large in a dim warm amber on black with the date under it,
// like a watch on its charger. Rendered a row at a time, without Arduino.
#pragma once
#include <stdint.h>

namespace nightstandview {

struct Layout {
  char time[8] = {0};
  char date[32] = {0};   // display bytes
  int timeTop = 0, dateTop = 0;
};

// `weekday` 0 = Sunday, `month` 1-12.
void layout(int hour, int minute, int weekday, int day, int month, uint8_t lang, Layout& out);
void renderRow(const Layout& l, int y, uint16_t* row);

}  // namespace nightstandview
