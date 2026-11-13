#include "Gfx.h"
#include "config.h"
#include "Platform.h"
#include "SunTimes.h"
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include "features/notify/NotifyView.h"     // gfxMessage reuses the notification layout
#include "ScreenView.h"
#include "SysText.h"

// The SmallTV's ST7789 has its CS line tied to GND and only latches SPI in
// **mode 3**. Arduino_GFX's stock Arduino_ST7789 forces SPI_MODE2 on the ESP8266
// (wrong clock edge for this panel), so the controller never initializes and the
// screen stays black even with the backlight on. Subclass begin() to force mode 3
// — matching the known-good GeekMagic community firmwares. (On ESP32 the base
// class already selects mode 3, so the override is harmless there.)

// Runtime panel colour order, read by the setRotation override below. The board
// header's TFT_BGR is the factory default for that variant; the Display tab can
// override it, because units of the same model do turn up with red and blue
// swapped in the controller.
static bool g_bgr = (TFT_BGR != 0);

class Arduino_ST7789_SmallTV : public Arduino_ST7789 {
 public:
  using Arduino_ST7789::Arduino_ST7789;   // inherit constructors
  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    _override_datamode = SPI_MODE3;
    return Arduino_TFT::begin(speed);
  }

  // Arduino_ST7789 hardcodes the MADCTL RGB order, so re-issue MADCTL with the
  // BGR bit (0x08) tracking g_bgr on every rotation change. The MX/MY/MV values
  // below are the base class's own mapping (ST7789_MADCTL_RGB is 0x00), so with
  // g_bgr false this writes exactly what the library would have written. Only
  // rotations 0-3 are used by the SmallTV (setRotation(r & 3)).
  void setRotation(uint8_t r) override {
    Arduino_TFT::setRotation(r);           // updates _rotation + width/height
    uint8_t madctl;
    switch (_rotation) {
      case 1:  madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MV; break;
      case 2:  madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MY; break;
      case 3:  madctl = ST7789_MADCTL_MY | ST7789_MADCTL_MV; break;
      default: madctl = 0; break;          // case 0
    }
    if (g_bgr) madctl |= 0x08;              // BGR
    _bus->beginWrite();
    _bus->writeC8D8(ST7789_MADCTL, madctl);
    _bus->endWrite();
  }
};

// ---- colour correction ----------------------------------------------------
// Per-channel gain in percent, 100 = untouched. Kept as plain bytes so the
// all-default case is a single comparison on the hot drawing path.
static uint8_t g_rGain = 100, g_gGain = 100, g_bGain = 100;

uint16_t gfxTint(uint16_t c) {
  if (g_rGain == 100 && g_gGain == 100 && g_bGain == 100) return c;
  uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  r = r * g_rGain / 100; if (r > 31) r = 31;
  g = g * g_gGain / 100; if (g > 63) g = 63;
  b = b * g_bGain / 100; if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static Arduino_DataBus* bus = nullptr;
static Arduino_GFX*     gfx = nullptr;

Arduino_GFX* gfxDev() { return gfx; }

// ---------------------------------------------------------------------------
void gfxBegin(const Settings& s) {
#ifdef TFT_PWR_PIN
  // Boards with a switched panel power rail (NM-TV-154): energize the display
  // before anything else or the panel never comes up.
  pinMode(TFT_PWR_PIN, OUTPUT);
  digitalWrite(TFT_PWR_PIN, TFT_PWR_ON);
#endif
  // Backlight off until the first screen is drawn: the panel's power-on noise
  // never shows, and main.cpp fades the splash in (gfxFadeSteps).
  pinMode(TFT_BL, OUTPUT);
  platformAnalogWriteInit(TFT_BL);
  gfxSetBrightness(0, s.backlightInverted);

  bus = new Arduino_HWSPI(TFT_DC, TFT_CS);   // ESP8266 HW-SPI (fixed SCLK/MOSI)
  // IPS=true so the panel colors are not inverted. The ST7789(V) has 240x320
  // RAM against 240x240 glass, so the rotation 2/3 row offset (TFT_ROW_OFFSET2,
  // 80) matters: without it a 180°-rotated image slides into the dead band.
  // Use the SmallTV variant so the SPI bus comes up in mode 3 (see class above).
  gfx = new Arduino_ST7789_SmallTV(bus, TFT_RST, 0 /*rotation*/, true /*IPS*/,
                                   TFT_WIDTH, TFT_HEIGHT,
                                   TFT_COL_OFFSET1, TFT_ROW_OFFSET1,
                                   TFT_COL_OFFSET2, TFT_ROW_OFFSET2);
  gfx->begin();
  // Colour order/inversion/gain before the first pixel, so the panel comes up
  // already corrected instead of flashing an uncorrected boot screen. This also
  // writes MADCTL for the configured rotation.
  gfxApplyColors(s);
  // Nothing in this UI ever wants wrapped text: overflowing labels used to
  // wrap around to x=0 on the next line (stray characters at the left edge).
  gfx->setTextWrap(false);
  gfx->fillScreen(C_BLACK);
}

void gfxSetBrightnessSteps(uint8_t steps, bool inverted) {
  analogWrite(TFT_BL, inverted ? 255 - steps : steps);
}

uint8_t gfxFadeLevel(uint8_t from, uint8_t to, int permille) {
  if (permille <= 0) return from;
  if (permille >= 1000) return to;
  const float t = permille / 1000.0f, e = t * t * (3 - 2 * t);   // ease in and out
  const float a = sqrtf(from), b = sqrtf(to), v = a + (b - a) * e;
  const int r = (int)(v * v + 0.5f);
  return (uint8_t)(r > 255 ? 255 : r);
}

void gfxFadeSteps(uint8_t from, uint8_t to, uint16_t ms, bool inverted) {
  const uint32_t t0 = millis();
  for (uint32_t t = 0; t < ms; t = millis() - t0) {
    gfxSetBrightnessSteps(gfxFadeLevel(from, to, (int)(t * 1000 / ms)), inverted);
    delay(8);
  }
  gfxSetBrightnessSteps(to, inverted);
}

void gfxSetBrightness(uint8_t pct, bool inverted) {
  gfxSetBrightnessSteps(brightnessSteps(pct), inverted);
}

void gfxSetRotation(uint8_t r) {
  if (gfx) gfx->setRotation(r & 3);
}

void gfxApplyColors(const Settings& s) {
  g_rGain = s.display.rGain;
  g_gGain = s.display.gGain;
  g_bGain = s.display.bGain;
  g_bgr = (s.display.colorOrder == COLOR_ORDER_BGR) ? true
        : (s.display.colorOrder == COLOR_ORDER_RGB) ? false
                                                    : (TFT_BGR != 0);
  if (!gfx) return;
  gfx->setRotation(s.rotation & 3);   // re-issues MADCTL with the new order
  gfx->invertDisplay(s.display.invert);
}

// ---- text: the system font (features/theme/SystemFont.h) -------------------
// Every string is drawn in SF Compact, one of the six system styles. Rows are
// composed in a small buffer and sent with one SPI write each, so text is drawn
// once, opaque on black, without flicker.
using gm::TextStyle;

static inline uint16_t mix(uint16_t bg, uint16_t fg, int a) {
  if (a <= 0) return bg;
  if (a >= 255) return fg;
  const int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
  const int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
  const int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

int gfxTextWidth(TextStyle style, const char* s) {
  return s ? gm::styleTextWidth(style, s, strlen(s)) : 0;
}

// `s` in display bytes, left edge at x, line top at `top`. `transparent` keeps
// the pixels under the text that the glyphs do not cover (other features draw
// over their own backgrounds); otherwise the line's box is painted `bg`.
static void drawStyled(TextStyle style, const char* s, int x, int top, uint16_t color, uint16_t bg, bool transparent) {
  if (!gfx || !s || !*s) return;
  const int w = gfxTextWidth(style, s), lh = gm::styleLineHeight(style);
  const int x0 = x < 0 ? 0 : x, x1 = (x + w > TFT_WIDTH) ? TFT_WIDTH : x + w;
  if (x1 <= x0) return;
  uint16_t row[TFT_WIDTH];
  uint8_t cover[TFT_WIDTH];
  gm::GlyphView g;
  for (int y = top; y < top + lh; y++) {
    if (y < 0 || y >= TFT_HEIGHT) continue;
    memset(cover, 0, sizeof(cover));
    int pen = x;
    for (const char* p = s; *p && pen < x1; p++) {
      gm::styleGlyph(style, (uint8_t)*p, g);
      const int gy = y - top - g.top;
      if (g.width && gy >= 0 && gy < g.height)
        for (int gx = 0; gx < g.width; gx++) {
          const int px = pen + g.left + gx;
          if (px >= x0 && px < x1) {
            const uint8_t a = gm::glyphCoverage(g, gx, gy);
            if (a > cover[px]) cover[px] = a;
          }
        }
      pen += g.advance;
    }
    if (!transparent) {
      for (int px = x0; px < x1; px++) row[px - x0] = mix(bg, color, cover[px]);
      gfx->draw16bitRGBBitmap(x0, y, row, x1 - x0, 1);
      continue;
    }
    for (int px = x0; px < x1;) {   // only the inked runs
      if (!cover[px]) { px++; continue; }
      const int start = px;
      while (px < x1 && cover[px]) { row[px - start] = mix(bg, color, cover[px]); px++; }
      gfx->draw16bitRGBBitmap(start, y, row, px - start, 1);
    }
  }
}

void gfxText(TextStyle style, const char* s, int x, int top, uint16_t color) {
  drawStyled(style, s, x, top, color, C_BLACK, false);
}
void gfxTextCentered(TextStyle style, const char* s, int top, uint16_t color) {
  int x = (TFT_WIDTH - gfxTextWidth(style, s)) / 2;
  if (x < 0) x = 0;
  drawStyled(style, s, x, top, color, C_BLACK, false);
}
// The largest style from `largest` down to Caption whose width fits `maxW`.
TextStyle gfxFitStyle(const char* s, int maxW, TextStyle largest) {
  for (int st = (int)largest; st > (int)TextStyle::Caption; st--)
    if (gfxTextWidth((TextStyle)st, s) <= maxW) return (TextStyle)st;
  return TextStyle::Caption;
}

// ---- the older size-based helpers, for features that still use them -------
// A scale of the old 6x8 font maps to the style nearest its height.
static TextStyle sizeStyle(uint8_t size) { return gm::textStyleForSize(size * GFX_FONT_H); }
int gfxTextW(const char* s, uint8_t size) { return gfxTextWidth(sizeStyle(size), s); }
void gfxDrawCentered(const char* s, int y, uint8_t size, uint16_t color) {
  int x = (TFT_WIDTH - gfxTextW(s, size)) / 2;
  if (x < 0) x = 0;
  drawStyled(sizeStyle(size), s, x, y, color, C_BLACK, true);
}
void gfxDrawText(const char* s, int x, int y, uint8_t size, uint16_t color, uint16_t bg) {
  drawStyled(sizeStyle(size), s, x, y, color, bg, bg == color);
}
uint8_t gfxFitSize(const char* s, int maxW, uint8_t maxSize) {
  for (uint8_t sz = maxSize; sz > 1; sz--)
    if (gfxTextW(s, sz) <= maxW) return sz;
  return 1;
}

// ---- system screens (laid out in ScreenView.cpp) ---------------------------
static void sendRow(void*, int y, const uint16_t* row) {
  uint16_t tinted[screenview::kWidth];
  for (int x = 0; x < screenview::kWidth; x++) tinted[x] = gfxTint(row[x]);
  gfx->draw16bitRGBBitmap(0, y, tinted, screenview::kWidth, 1);
}
static void showScreen(const screenview::Line* lines, int count, uint16_t accent) {
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);
  screenview::render(lines, count, accent, sendRow, nullptr);
}

static screenview::Splash g_splash;

static void splashRows(int y0, int y1) {
  uint16_t row[screenview::kWidth];
  for (int y = y0; y < y1; y++) {
    if (y < 0 || y >= screenview::kHeight) continue;
    screenview::splashRow(g_splash, y, row);
    sendRow(nullptr, y, row);
  }
}

void gfxSplash(const char* detail, int pct) {
  if (!gfx) return;
  screenview::splashLayout(detail, pct, g_splash);
  gfx->fillScreen(C_BLACK);
  splashRows(g_splash.top, g_splash.bottom);
}

void gfxSplashUpdate(const char* detail, int pct) {
  if (!gfx) return;
  if (pct > 100) pct = 100;
  const int from = g_splash.pct;
  if (detail) {
    const int oldBottom = g_splash.bottom;
    char keep[sizeof(g_splash.detail)];
    memcpy(keep, g_splash.detail, sizeof(keep));
    screenview::splashLayout(detail, from, g_splash);
    if (strcmp(keep, g_splash.detail) != 0)
      splashRows(g_splash.detailTop - 6, oldBottom > g_splash.bottom ? oldBottom : g_splash.bottom);
  }
  if (pct == from) return;
  // A jump of more than a few percent slides over about 150 ms; small steps
  // (an upload's progress) are drawn as they come.
  const int frames = (pct > from + 3 && from >= 0) ? 8 : 1;
  for (int i = 1; i <= frames; i++) {
    g_splash.pct = from + (pct - from) * i / frames;
    splashRows(g_splash.barTop, g_splash.barTop + screenview::kBarH);
    if (i < frames) delay(18);
  }
}

int gfxApInfo(const char* ssid, const char* pass, const char* ip, const char* why, int page) {
  char payload[160];
  qr::Code code;
  const bool hasQr = qr::wifiPayload(ssid, pass, payload, sizeof(payload)) &&
                     qr::encode((const uint8_t*)payload, strlen(payload), code);
  if (hasQr && page % 2 == 0) {
    if (gfx) {
      gfx->fillScreen(C_BLACK);
      screenview::QrPage p;
      screenview::qrLayout(code, p);
      char head[48], cap[48];
      screenview::renderQr(p, sys::text(sys::WifiSetup, head, sizeof(head)), sys::text(sys::ScanToJoin, cap, sizeof(cap)),
                           screenview::kWarn, sendRow, nullptr);
    }
  } else {
    screenview::Line l[screenview::kMaxLines];
    screenview::Scratch scratch;
    showScreen(l, screenview::setupLines(ssid, pass, ip, why, scratch, l), screenview::kWarn);
  }
  return hasQr ? 2 : 1;
}

void gfxStaInfo(const char* ssid, const char* ip, const char* host) {
  screenview::Line l[screenview::kMaxLines];
  screenview::Scratch scratch;
  showScreen(l, screenview::connectedLines(ssid, ip, host, scratch, l), screenview::kOk);
}

// A short message: the notification screen's layout (dot and title, the text
// wrapped below), without the time-left line.
void gfxMessage(const char* title, const char* msg, uint16_t titleColor) {
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);
  char t[NOTIFY_TITLE_MAX + 1], m[NOTIFY_LABEL_MAX + 1];
  gm::displayTextTo(t, sizeof(t), title ? title : "");
  gm::displayTextTo(m, sizeof(m), msg ? msg : "");
  notifyview::Layout l;
  notifyview::layout(t, m, l);
  uint16_t row[notifyview::kWidth];
  for (int y = l.top; y < l.bottom; y++) {
    if (!notifyview::renderRow(l, titleColor, y, row)) continue;
    for (int x = 0; x < notifyview::kWidth; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, notifyview::kWidth, 1);
  }
}

// Shown in safe mode after an exception reset: the crash PC and fault address
// stay readable, with the IP to flash a fix over the air.
void gfxCrash(const char* epc, const char* addr, const char* ip) {
  screenview::Line l[screenview::kMaxLines];
  screenview::Scratch scratch;
  showScreen(l, screenview::crashLines(epc, addr, ip, scratch, l), screenview::kBad);
}
