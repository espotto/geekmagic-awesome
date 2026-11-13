// WeatherView.h — the Weather app's screen, in the manner of a phone's weather
// app: a sky gradient (blue by day, deep blue at night), the temperature large,
// the condition and the day's high and low under it, and a translucent card
// with the next hours: time, a drawn icon, temperature. Rendered a row at a
// time, without Arduino (tests/weather).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace weatherview {

constexpr int kHours = 4;   // "Now" and the next three

struct Hour {
  uint8_t hour = 0;     // 0-23, local
  int8_t temp = 0;      // whole degrees C
  uint8_t code = 0;     // WMO weather code
  bool day = true;
};

struct Data {
  bool valid = false;
  float temp = 0, high = 0, low = 0;
  float feels = 0, wind = 0;   // apparent temperature (C), wind (km/h): the web panel shows them
  uint8_t humidity = 0;        // %
  uint8_t code = 0;
  bool day = true;
  Hour hours[kHours];
};

// What the screen says when there is no data: the location is not set yet,
// or the first fetch has not come back.
enum class Empty : uint8_t { None, NoLocation, Loading };

struct Layout {
  uint8_t lang = 0;
  bool day = true, valid = false;
  char temp[12] = {0};       // display bytes, e.g. "-3°"
  char cond[40] = {0};
  bool condSmall = false;    // the condition in Caption: too long for Body
  char hilo[40] = {0};
  char hourText[kHours][8] = {{0}};
  char hourTemp[kHours][8] = {{0}};
  uint8_t hourIcon[kHours] = {0};
  bool hourDay[kHours] = {false};
  int tempTop = 0, condTop = 0, hiloTop = 0, cardTop = 0, cardBottom = 0;
};

// The icon drawn for a WMO code (also used by the tests).
enum Icon : uint8_t { Sun, Moon, PartSun, PartMoon, Cloud, Fog, Rain, Snow, Storm };
Icon iconFor(uint8_t code, bool day);
// The condition's index in the word table (0-11).
int conditionFor(uint8_t code);
// The condition in words, in a language (gm::languageCode index), UTF-8.
void conditionText(uint8_t code, uint8_t lang, char* out, size_t cap);

void layout(const Data& d, Empty empty, uint8_t lang, Layout& out);
void renderRow(const Layout& l, int y, uint16_t* row);

}  // namespace weatherview
