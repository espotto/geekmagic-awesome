// Gfx.h — shared ST7789 device, drawing primitives, and boot/status screens.
//
// This is the core display layer. The apps (watch faces, Calendar, Weather, ...)
// render on top of it via gfxDev() and the exposed text helpers; each feature owns
// its own feature-specific rendering. Nothing feature-specific lives here.
#pragma once
#include <Arduino.h>
#include "Settings.h"
#include "features/theme/SystemFont.h"

class Arduino_GFX;   // fwd-decl: only the drawing .cpp files pull in the full lib

// ---- Shared colors (RGB565) ----------------------------------------------
// The SmallTV variants ship visibly different panels: some run warm, some cold,
// and a few have red and blue swapped in the controller. gfxTint() applies the
// per-channel gain set in the Display tab to every colour on its way to the
// panel, so a unit can be matched to the others without touching each renderer.
// Colours computed inside a feature go through it too (the apps).
uint16_t gfxTint(uint16_t rgb565);

#define C_BLACK  gfxTint(0x0000)
#define C_WHITE  gfxTint(0xFFFF)
#define C_GREEN  gfxTint(0x07E0)
#define C_RED    gfxTint(0xF800)
#define C_GRAY   gfxTint(0x8410)
#define C_DGRAY  gfxTint(0x4208)
#define C_YELLOW gfxTint(0xFFE0)
#define C_BLUE   gfxTint(0x041F)

// ---- Device lifecycle -----------------------------------------------------
void         gfxBegin(const Settings& s);
void         gfxSetBrightness(uint8_t pct, bool inverted);
void         gfxSetBrightnessSteps(uint8_t steps, bool inverted);   // 0..255, the finest the PWM has
// The level `permille` (0..1000) of the way from `from` to `to`, eased and
// moved in a perceptual (square-root) space so a fade looks even to the eye.
uint8_t      gfxFadeLevel(uint8_t from, uint8_t to, int permille);
// A blocking backlight fade over `ms`, for start-up and restart.
void         gfxFadeSteps(uint8_t from, uint8_t to, uint16_t ms, bool inverted);
void         gfxSetRotation(uint8_t r);
// Push the Display tab's colour settings to the panel: MADCTL colour order,
// the inversion bit, and the per-channel gain gfxTint() applies. Callers repaint
// afterwards — already-drawn pixels keep the previous correction.
void         gfxApplyColors(const Settings& s);
Arduino_GFX* gfxDev();                 // shared draw target for feature renderers

// ---- Text: the system font (features/theme/SystemFont.h) -------------------
// Strings are display bytes (gm::displayText). Drawn opaque on black.
int     gfxTextWidth(gm::TextStyle style, const char* s);
void    gfxText(gm::TextStyle style, const char* s, int x, int top, uint16_t color);
void    gfxTextCentered(gm::TextStyle style, const char* s, int top, uint16_t color);
gm::TextStyle gfxFitStyle(const char* s, int maxW, gm::TextStyle largest);

// The older helpers by scale of the 6x8 font (1-6), kept for features that use
// them: each scale draws in the system style nearest its height. `y` is the top
// of the line; gfxDrawText is opaque when `bg` differs from `color`.
#define GFX_FONT_W 6
#define GFX_FONT_H 8
int     gfxTextW(const char* s, uint8_t size);
void    gfxDrawCentered(const char* s, int y, uint8_t size, uint16_t color);
void    gfxDrawText(const char* s, int x, int y, uint8_t size, uint16_t color, uint16_t bg);
uint8_t gfxFitSize(const char* s, int maxW, uint8_t maxSize);

// ---- Shared boot / status / diagnostic screens ----------------------------
// The splash (ScreenView): the wordmark, a progress bar (`pct` 0-100, or -1
// for none) and a grey detail line. gfxSplash paints the whole screen;
// gfxSplashUpdate repaints only the bar, sliding it to `pct`, and the detail
// line when `detail` is not null, so nothing flickers.
void gfxSplash(const char* detail, int pct);
void gfxSplashUpdate(const char* detail, int pct);
// The setup screen. `page` 0 shows the QR code for joining the hotspot, 1 the
// network, password and address as text; returns the page count (1 when no
// code fits). `why` (optional): the last failed join, on the text page.
int  gfxApInfo(const char* ssid, const char* pass, const char* ip, const char* why = nullptr, int page = 1);
void gfxStaInfo(const char* ssid, const char* ip, const char* host);
void gfxMessage(const char* title, const char* msg, uint16_t titleColor);
void gfxCrash(const char* epc, const char* addr, const char* ip);  // safe-mode diag
