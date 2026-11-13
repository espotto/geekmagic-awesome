// The Weather app's screen (WeatherView) on the desktop: WMO codes map to
// conditions and icons, text stays inside the safe area, and with a path
// argument sample screens are written as a PPM (tests/apps/run.sh).
#include "weather/WeatherView.h"
#include "ppm.h"
#include <cassert>
#include <iostream>
using namespace weatherview;

#include "AppDraw.h"
// The condition and the high/low line stay inside the safe area.
static bool appdraw_ok(const Layout& l) {
  const gm::TextStyle st = l.condSmall ? gm::TextStyle::Caption : gm::TextStyle::Body;
  return appdraw::width(st, l.cond) <= 200 && appdraw::width(gm::TextStyle::Caption, l.hilo) <= 200;
}
static std::vector<uint16_t> frame(const Layout& l) {
  std::vector<uint16_t> px(240 * 240);
  for (int y = 0; y < 240; ++y) renderRow(l, y, &px[y * 240]);
  return px;
}

int main(int argc, char** argv) {
  assert(conditionFor(0) == 0 && conditionFor(3) == 3 && conditionFor(61) == 6 && conditionFor(75) == 8 && conditionFor(96) == 11);
  assert(iconFor(0, true) == Sun && iconFor(0, false) == Moon && iconFor(2, true) == PartSun && iconFor(71, true) == Snow);
  Data d;
  d.valid = true; d.temp = -2.6f; d.high = 1.2f; d.low = -5.4f; d.code = 2; d.day = true;
  const uint8_t codes[kHours] = {2, 61, 71, 95};
  for (int i = 0; i < kHours; ++i) { d.hours[i].hour = static_cast<uint8_t>(13 + i); d.hours[i].temp = static_cast<int8_t>(-3 + i); d.hours[i].code = codes[i]; d.hours[i].day = true; }
  Layout l;
  layout(d, Empty::None, 1, l);
  assert(l.cardBottom <= 240 - 20 && l.tempTop >= 20);
  std::vector<std::vector<uint16_t>> frames;
  frames.push_back(frame(l));
  Data n = d;
  n.day = false; n.code = 0; n.temp = 1.4f;
  const uint8_t night[kHours] = {0, 1, 45, 80};
  for (int i = 0; i < kHours; ++i) { n.hours[i].code = night[i]; n.hours[i].day = false; n.hours[i].hour = static_cast<uint8_t>((22 + i) % 24); }
  layout(n, Empty::None, 1, l);
  frames.push_back(frame(l));
  layout(d, Empty::None, 0, l);
  frames.push_back(frame(l));
  layout(Data(), Empty::NoLocation, 1, l);
  frames.push_back(frame(l));
  // Every language's longest condition fits the width.
  for (uint8_t lang = 0; lang < 2; ++lang)
    for (uint8_t code : {0, 1, 2, 3, 45, 51, 56, 61, 71, 80, 85, 95}) {
      Data e = d; e.code = code;
      layout(e, Empty::None, lang, l);
      assert(appdraw_ok(l));
    }
  if (argc > 1) writeFrames(argv[1], frames);
  std::cout << "weather view tests passed\n";
}
