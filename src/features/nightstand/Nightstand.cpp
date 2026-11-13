#include "Nightstand.h"
#include <Arduino_GFX_Library.h>
#include "Clock.h"
#include "Gfx.h"
#include "NightstandView.h"
#include "../theme/Languages.h"

bool nightstandWanted(const Settings& s) {
  if (!s.clock.nightstand) return false;
  struct tm t;
  if (!clockNow(t)) return false;
  return clockNightActive() || clockSunNight(s);
}

void nightstandService(const Settings& s, bool force) {
  static int32_t drawn = -1;
  struct tm t;
  if (!clockNow(t)) return;
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const int32_t key = (t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min) * 8 + lang;
  if (!force && key == drawn) return;
  drawn = key;
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  nightstandview::Layout l;
  nightstandview::layout(t.tm_hour, t.tm_min, t.tm_wday, t.tm_mday, t.tm_mon + 1, lang, l);
  uint16_t row[240];
  for (int y = 0; y < 240; y++) {
    nightstandview::renderRow(l, y, row);
    for (int x = 0; x < 240; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, 240, 1);
    if ((y & 15) == 15) yield();
  }
}
