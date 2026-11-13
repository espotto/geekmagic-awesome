// CalendarView.h — the Calendar app's month screen, laid out and rendered
// without Arduino or the display driver, so the desktop can test and preview
// it (tests/calendar).
//
// The look is a phone's month view: the month's name in red with the year in
// grey beside it, a row of weekday initials, and the days in a grid, weekends
// in grey and today white on a red circle. The week starts on Monday, except
// in English (Sunday). Everything stays at least kSafe px inside the panel,
// which the device's case covers at the edges. Text is in the system font.
#pragma once
#include <stdint.h>

namespace calendarview {

constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kSafe = 20;
constexpr uint16_t kRed  = 0xFA27;    // #ff453a
constexpr uint16_t kText = 0xF7BE;    // #f5f5f7
constexpr uint16_t kGrey = 0x8C72;    // #8e8e93
constexpr uint16_t kDim  = 0x632C;    // #636366, the weekday initials

struct Layout {
  int year = 0, month = 0, today = 0;   // month 1-12; today 0 = not this month
  uint8_t lang = 0;                     // gm::languageCode index
  int days = 0;                         // in the month
  int firstCol = 0;                     // column of the 1st, 0-6
  int rows = 0;                         // weeks shown, 4-6
  int top = 0, bottom = 0;              // rows the screen uses
  int headTop = 0, initialsTop = 0, gridTop = 0, rowH = 0;
  char title[24] = {0};                 // display bytes: month name
  char yearText[8] = {0};
  bool smallTitle = false;              // the month in Body: too long for Headline beside the year
};

// Days in a month and the weekday of a date (0 = Sunday), Gregorian.
int daysIn(int year, int month);
int weekday(int year, int month, int day);

void layout(int year, int month, int today, uint8_t lang, Layout& out);
// Screen row `y` into `row` (kWidth RGB565 pixels, black around the content).
void renderRow(const Layout& l, int y, uint16_t* row);
// The grid cell of `day`: its centre. Exposed for tests.
void cellCentre(const Layout& l, int day, int& x, int& y);

}  // namespace calendarview
