// RemindersView.h — the Reminders app's screen, laid out and rendered without
// Arduino or the display driver (tests/reminders).
//
// The look is a list in a phone's Reminders app: the list's name in blue with
// the count of open items beside it, then one row per item: an empty ring for
// an open one, a filled blue ring for a done one (its text grey), hairlines
// between rows. Open items come first. What does not fit ends in "+N more";
// an empty list says so. All of it at least kSafe px inside the panel.
#pragma once
#include <stdint.h>
#include "RemindersList.h"

namespace remindersview {

constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kSafe = 20;
constexpr int kRowH = 30;
constexpr uint16_t kBlue = 0x0C3F;   // #0a84ff
constexpr uint16_t kText = 0xF7BE;   // #f5f5f7
constexpr uint16_t kGrey = 0x8C72;   // #8e8e93
constexpr uint16_t kRing = 0x632C;   // #636366
constexpr uint16_t kLine = 0x2965;   // #2c2c2e

struct Layout {
  uint8_t lang = 0;
  int shown = 0;                      // item rows on screen
  int more = 0;                       // items left out ("+N more")
  int order[reminders::kMax] = {0};   // list index of each row, open items first
  int titleTop = 0, listTop = 0;
  bool smallTitle = false;
  char title[24] = {0};               // display bytes
  char count[8] = {0};
  char moreText[24] = {0};
  char empty[32] = {0};
  char text[reminders::kMax][48] = {{0}};   // each row's text, display bytes, cut to fit
  bool done[reminders::kMax] = {false};
  bool small[reminders::kMax] = {false};   // the row in Caption: too long for Body
};

void layout(const reminders::List& list, uint8_t lang, Layout& out);
// Screen row `y` into `row` (kWidth RGB565 pixels, black around the content).
void renderRow(const Layout& l, int y, uint16_t* row);

}  // namespace remindersview
