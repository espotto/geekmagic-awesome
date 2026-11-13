#include "NightstandView.h"
#include <stdio.h>
#include "../AppDraw.h"

namespace nightstandview {
namespace {
using gm::TextStyle;
// Short weekday names (Sunday first) and short months, per language.
const char kDays[] PROGMEM =
  "Sun|Mon|Tue|Wed|Thu|Fri|Sat|"
  "Вс|Пн|Вт|Ср|Чт|Пт|Сб";
const char kMonths[] PROGMEM =
  "Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec|"
  "января|февраля|марта|апреля|мая|июня|июля|августа|сентября|октября|ноября|декабря";
// A dim warm amber, kind to eyes in the dark, and a dimmer one for the date.
const uint16_t kTime = appdraw::rgb(200, 96, 8);
const uint16_t kDate = appdraw::rgb(110, 52, 4);
}  // namespace

void layout(int hour, int minute, int weekday, int day, int month, uint8_t lang, Layout& out) {
  out = Layout();
  if (lang > 6) lang = 0;
  snprintf(out.time, sizeof(out.time), "%d:%02d", hour, minute);
  char wd[16], mo[24], utf8[48];
  appdraw::entry(kDays, lang * 7 + (weekday % 7), wd, sizeof(wd));
  appdraw::entry(kMonths, lang * 12 + ((month + 11) % 12), mo, sizeof(mo));
  if (lang == 0) snprintf(utf8, sizeof(utf8), "%s, %s %d", wd, mo, day);
  else snprintf(utf8, sizeof(utf8), "%s, %d %s", wd, day, mo);
  gm::displayTextTo(out.date, sizeof(out.date), utf8);
  const int th = gm::styleLineHeight(TextStyle::DisplayM), dh = gm::styleLineHeight(TextStyle::Body);
  out.timeTop = (appdraw::kHeight - th - dh) / 2;
  out.dateTop = out.timeTop + th - 4;
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  for (int x = 0; x < appdraw::kWidth; ++x) row[x] = 0;
  appdraw::textCentered(TextStyle::DisplayM, l.time, 120, l.timeTop, y, kTime, row);
  appdraw::textCentered(TextStyle::Body, l.date, 120, l.dateTop, y, kDate, row);
}

}  // namespace nightstandview
