#include "CalendarMode.h"
#include <Arduino_GFX_Library.h>
#include "Clock.h"
#include "Gfx.h"
#include "SysText.h"
#include "CalendarView.h"
#include "../theme/Languages.h"

CalendarMode g_calendarMode;

void CalendarMode::service(const Settings& s) {
  struct tm t;
  if (!clockNow(t)) {
    if (!waiting_) {
      {
        char title[32], msg[48];
        gfxMessage(sys::text(sys::Calendar, title, sizeof(title)), sys::text(sys::WaitingForTime, msg, sizeof(msg)), 0xFA27);
      }
      waiting_ = true;
      drawn_ = 0;
    }
    return;
  }
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = ((uint32_t)(t.tm_year + 1900) * 13 + t.tm_mon + 1) * 32 * 8 + t.tm_mday * 8 + lang;
  if (key == drawn_) return;
  drawn_ = key;
  waiting_ = false;

  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  calendarview::Layout l;
  calendarview::layout(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, lang, l);
  gfx->fillScreen(C_BLACK);
  uint16_t row[calendarview::kWidth];
  for (int y = l.top - 4; y < l.bottom + 4; y++) {
    calendarview::renderRow(l, y, row);
    for (int x = 0; x < calendarview::kWidth; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, calendarview::kWidth, 1);
    if ((y & 15) == 15) yield();
  }
}
