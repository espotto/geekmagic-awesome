// geekmagic — firmware for the GeekMagic SmallTV-Ultra (ESP-12F / ESP8266)
//
// Apps, each a self-contained DisplayMode (see Mode.h), picked in the web UI
// and dispatched from the registry below: watch faces (features/theme),
// Calendar, Reminders, Weather, Timer and Pomodoro;
// notifications, the timer and the nightstand go over them.
// Shared plumbing (WiFi, web UI, OTA, display core, settings) lives at src root.
//
// License: WTFPL
#include <Arduino.h>
#include "Platform.h"
#include "CrashRecord.h"
#include "config.h"
#include "Settings.h"
#include "SysText.h"
#include "features/theme/Languages.h"
#include "Net.h"
#include "Gfx.h"
#include "WebPortal.h"
#include "Mode.h"
#include "Clock.h"
#include "SunTimes.h"
#include "features/theme/ThemeMode.h"
#include "NotifyMode.h"
#include "CalendarMode.h"
#include "RemindersMode.h"
#include "WeatherMode.h"
#include "TimerMode.h"
#include "PomodoroMode.h"
#include "Nightstand.h"


// ---- mode registry --------------------------------------------------------
// The compiled-in features, in display order. main.cpp holds no per-feature
// state of its own — each mode owns its fetch/render/dirty tracking.
static DisplayMode* kModes[] = {
  &g_themeMode,
  &g_calendarMode,
  &g_remindersMode,
  &g_weatherMode,
  &g_timerMode,
  &g_pomodoroMode,
};
static const size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);

// ---- carousel -------------------------------------------------------------
// MODE_CAROUSEL rotates through the ticked features. Switches call wake() on
// the incoming mode: repaint from cached data, no refetch.
static size_t   g_carIdx = 0;
static uint32_t g_carSwitch = 0;
static bool     g_carPaused = false;   // button paused the auto-rotation (stays put)

static bool carouselHas(const Settings& s, const DisplayMode* m) {
  switch (m->modeConst()) {
    case MODE_THEME:  return s.carouselTheme;
    case MODE_CALENDAR: return s.carouselCalendar;
    case MODE_REMINDERS: return s.carouselReminders;
    case MODE_WEATHER: return s.carouselWeather;
    case MODE_TIMER: return s.carouselTimer;
    case MODE_POMODORO: return s.carouselPomodoro;
    default:          return true;
  }
}

static void fadeSwitch(DisplayMode* m, const Settings& s);   // below, next to the backlight

// Advance g_carIdx to the next ticked mode (stays put if none other is ticked).
static void carouselNext(const Settings& s) {
  for (size_t hop = 1; hop <= kModeCount; hop++) {
    size_t cand = (g_carIdx + hop) % kModeCount;
    if (!carouselHas(s, kModes[cand])) continue;
    if (cand != g_carIdx) {
      g_carIdx = cand;
      fadeSwitch(kModes[cand], s);
    }
    return;
  }
}

static DisplayMode* activeMode(const Settings& s) {
  if (s.mode == MODE_CAROUSEL && kModeCount > 0) {
    if (g_carSwitch == 0) g_carSwitch = millis();
    if (!carouselHas(s, kModes[g_carIdx])) carouselNext(s);   // settings changed
    bool dwellExpired = millis() - g_carSwitch >= (uint32_t)s.carouselSec * 1000UL;
    if (g_carPaused) dwellExpired = false;   // button froze the rotation on the current page
    if (dwellExpired) {
      g_carSwitch = millis();
      carouselNext(s);
    }
    return kModes[g_carIdx];
  }
  for (size_t i = 0; i < kModeCount; i++)
    if (kModes[i]->modeConst() == s.mode) return kModes[i];
  return kModeCount ? kModes[0] : nullptr;   // fall back to the first compiled mode
}

static Settings g_settings;
static String   g_resetReason;        // why the chip last reset (diagnostics)
static bool     g_safeMode = false;   // last reset was an exception -> don't re-enter the crash
static char     g_epcStr[16] = "";
static char     g_addrStr[16] = "";
static int g_lastBr = -1;        // effective brightness the backlight heads for (-1 = none yet)
static int g_shownBr = 0;        // level the backlight shows now, moving towards g_lastBr
static int g_fadeFrom = 0;       // where the running fade started, and when
static uint32_t g_fadeAt = 0;
static const uint16_t kFadeMs = 450;   // every brightness change fades over this
static uint32_t g_btnWakeUntil = 0;  // button wake overrides dimming until this millis()
#if HAS_LDR
static uint32_t g_lastAutoBr = 0;
static uint8_t  g_ldrCache   = DEFAULT_BRIGHTNESS;   // last LDR reading (2 s cadence)
#endif

// Single brightness resolver: night mode overrides auto-brightness overrides the
// manual level. Only writes the PWM when the effective target changes.
// The order is in resolveBrightness() (SunTimes.cpp), which the native tests
// cover. The result is in backlight steps (0..255) so the sun fade can move as
// finely as the PWM allows.
static uint8_t appEffectiveBrightness() {
  BrightnessInputs in = {};
  // A button wake press briefly lifts the screen to the manual level so a touch
  // reveals it even when night mode has dimmed or blanked the backlight.
  in.wake = g_btnWakeUntil && millis() < g_btnWakeUntil;
  in.sliderPct = g_settings.brightness;
  in.nightActive = clockNightActive();
  in.nightPct = g_settings.clock.nightLevel;
  in.overlap = g_settings.clock.sunOverlap;
  in.sunValid = clockSunSteps(g_settings, brightnessSteps(g_settings.brightness), in.sunSteps);
#if HAS_LDR
  if (g_settings.autoBrightness) {
    if (millis() - g_lastAutoBr > 2000) {
      g_lastAutoBr = millis();
      int raw = analogRead(LDR_PIN);
      g_ldrCache = (uint8_t)constrain(raw * 100 / ADC_MAX, 5, 100);
    }
    in.ldrOn = true;
    in.ldrPct = g_ldrCache;
  }
#endif
  return resolveBrightness(in);
}

// The lowest free heap the loop has seen since boot, for /api/status.
static uint32_t g_minHeap = UINT32_MAX;
uint32_t appMinHeap() { return g_minHeap == UINT32_MAX ? 0 : g_minHeap; }

// A rotation step: the backlight fades out, the next app draws in the dark,
// and the backlight fades back in, so no half-drawn screen ever shows.
static void fadeSwitch(DisplayMode* m, const Settings& s) {
  const uint8_t level = g_shownBr < 0 ? 0 : (uint8_t)g_shownBr;
  gfxFadeSteps(level, 0, 180, s.backlightInverted);
  m->wake(s);
  m->service(s);
  gfxFadeSteps(0, level, 260, s.backlightInverted);
}

int appBacklightSteps() { return g_lastBr; }   // the level set, 0..255 (-1: none yet)

// A new target starts a fade from wherever the backlight is; each call (every
// loop pass) moves it along, so night mode, the sun and the slider all glide.
void appApplyBrightness() {
  uint8_t t = appEffectiveBrightness();
  if ((int)t != g_lastBr) {
    g_lastBr = t;
    g_fadeFrom = g_shownBr;
    g_fadeAt = millis();
  }
  if (g_shownBr == g_lastBr) return;
  const uint32_t el = millis() - g_fadeAt;
  const int v = el >= kFadeMs ? g_lastBr : gfxFadeLevel(g_fadeFrom, g_lastBr, (int)(el * 1000 / kFadeMs));
  if (v != g_shownBr) {
    g_shownBr = v;
    gfxSetBrightnessSteps(v, g_settings.backlightInverted);
  }
}

#ifdef BTN_PIN
// Capacitive touch button (e.g. NM-TV-154, GPIO32). 
// touchRead() returns a capacitance count that drops when a finger is present; the press
// threshold is calibrated from an idle baseline (recomputed if the configured
// sensitivity changes), since counts vary per unit. A short and a long press
// each run a configurable action (Settings.button). Non-blocking except for the
// deliberate ~1 s screen-off blink.
static uint32_t g_btnInfoUntil = 0;   // network-info screen shown until this millis()

static void buttonBlinkOff() {
  gfxSetBrightness(0, g_settings.backlightInverted);   // blank the panel
  delay(1000);
  g_shownBr = 0;
  g_lastBr = -1;                       // force the next tick to fade brightness back
  appApplyBrightness();
}

static void buttonShowNetInfo() {
  gfxStaInfo(netSSID().c_str(), netIP().c_str(), g_settings.hostname.c_str());
  g_btnInfoUntil = millis() + BTN_INFO_MS;
}

static void buttonWake() {
  g_btnWakeUntil = millis() + BTN_WAKE_MS;  // the brightness resolver lifts dimming for this window
  g_lastBr = -1;                            // force the next tick to apply it
  appApplyBrightness();
}

static void buttonAckNotify() {
  g_notifyMode.dismiss();   // expire the active overlay now; the next one (or the feature) takes over
}

static void buttonGoHome() {
  g_carPaused = false;                 // resume normal rotation
  if (g_settings.mode == MODE_CAROUSEL && kModeCount > 0) {
    g_carIdx = 0;
    g_carSwitch = millis();
    if (!carouselHas(g_settings, kModes[0])) carouselNext(g_settings);  // 0 not ticked -> first ticked
    else kModes[0]->wake(g_settings);
  } else {
    DisplayMode* m = activeMode(g_settings);
    if (m) m->invalidate(g_settings);
  }
}

static void buttonNext() {
  if (g_settings.mode != MODE_CAROUSEL || kModeCount == 0) return;
  g_carPaused = true;                  // manual navigation stops the auto-rotation
  g_carSwitch = millis();
  carouselNext(g_settings);            // advance + wake the incoming mode
}

static void buttonAction(uint8_t act) {
  switch (act) {
    case BTN_ACT_SCREEN_OFF: buttonBlinkOff();    break;
    case BTN_ACT_HOME:       buttonGoHome();      break;
    case BTN_ACT_NEXT:       buttonNext();        break;
    case BTN_ACT_NETINFO:    buttonShowNetInfo(); break;
    case BTN_ACT_WAKE:       buttonWake();        break;
    case BTN_ACT_ACK:        buttonAckNotify();   break;
    default: break;                                 // BTN_ACT_NONE
  }
}

static void buttonService() {
  static bool     inited   = false;
  static uint16_t thrDown  = 0;       // press  when median < thrDown
  static uint16_t thrUp    = 0;       // release when median > thrUp (hysteresis band)
  static uint8_t  sensSeen = 0;       // sensitivity the thresholds were computed for
  static bool     stable   = false;   // confirmed (debounced) button state
  static uint8_t  run      = 0;       // consecutive samples disagreeing with 'stable'
  static uint32_t downAt   = 0;

  // Calibrate (or recalibrate when the web UI changes the sensitivity).
  if (!inited || sensSeen != g_settings.button.sensitivity) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) { sum += touchRead(BTN_PIN); delay(2); }
    uint16_t baseline = sum / 16;
    uint8_t s = g_settings.button.sensitivity;
    thrDown = (uint16_t)((uint32_t)baseline * s / 100);
    uint8_t up = s + 5; if (up > 99) up = 99;            // ~5 % hysteresis band
    thrUp = (uint16_t)((uint32_t)baseline * up / 100);
    sensSeen = s;
    inited = true;
  }

  // ESP32 touchRead() emits isolated low-going glitch samples; a median of three
  // reads rejects any single spike, so a lone glitch can no longer be seen as a
  // press. Hysteresis then keeps the state from chattering near the threshold.
  uint16_t a = touchRead(BTN_PIN);
  uint16_t b = touchRead(BTN_PIN);
  uint16_t c = touchRead(BTN_PIN);
  uint16_t m = a > b ? (b > c ? b : (a > c ? c : a))
                     : (a > c ? a : (b > c ? c : b));

  bool wantDown = stable ? (m < thrUp) : (m < thrDown);
  if (wantDown == stable) { run = 0; return; }

  // Integrator debounce: require BTN_CONFIRM consecutive agreeing samples before
  // flipping the confirmed state, so a short burst of noise is ignored too.
  if (++run < BTN_CONFIRM) return;
  run = 0;
  stable = wantDown;
  if (stable) { downAt = millis(); return; }   // press start: decide on release
  uint32_t held = millis() - downAt;
  buttonAction(held >= BTN_LONG_MS ? g_settings.button.longAction
                                   : g_settings.button.shortAction);
}
#endif

// Exposed to the web portal (/api/status) so the last reset reason is visible.
const char* appResetReason() { return g_resetReason.c_str(); }

// Called by the web portal after settings are applied: re-init every mode and
// force a fresh repaint so a mode/URL/symbol change takes effect immediately.
void appInvalidate() {
  for (size_t i = 0; i < kModeCount; i++) kModes[i]->invalidate(g_settings);
}

// Called by the web portal before a request that needs a lot of heap, such as
// a settings save: every mode frees what it can rebuild on its next service().
void appRelease() {
  for (size_t i = 0; i < kModeCount; i++) kModes[i]->freeForRequest();
}

static void bootProgress(const char* msg, uint8_t pct) {
  gfxSplashUpdate(msg, pct);
}

// The setup screen alternates between its QR code and the same details as
// text; gfxApInfo says whether there are two pages.
static uint8_t  g_apPage = 0, g_apPages = 1;
static uint32_t g_apPageAt = 0;
static const uint32_t kApPageMs = 8000;
static void showApInfo(bool restart) {
  if (restart) g_apPage = 0;
  g_apPages = gfxApInfo(g_settings.apSsid.c_str(), g_settings.apPass.c_str(), netIP().c_str(),
                        netFailReason(), g_apPage);
  g_apPageAt = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(FW_NAME " " FW_VERSION);

  // Capture why we (re)booted. On a reboot loop this is the key clue, and the
  // device's UART isn't exposed — so we also show it on screen below. On the
  // ESP8266 we also keep the crash PC (epc1) for addr2line decoding; the
  // ESP32-C2 (RISC-V) doesn't expose it, so epc/addr come back empty there.
  PlatformReset pr = platformResetInfo();
  platformCrashRecordBegin();   // keep the registers/uptime of an abnormal reset in RTC memory
  Serial.print("[boot] reset reason: ");
  Serial.println(pr.reason);

  if (pr.wasCrash) {
    g_safeMode = true;                   // crashed last boot -> stay out of the crash path
    strlcpy(g_epcStr,  pr.epc,  sizeof(g_epcStr));
    strlcpy(g_addrStr, pr.addr, sizeof(g_addrStr));
    char rich[80];
    snprintf(rich, sizeof(rich), "%s epc %s addr %s", pr.reason.c_str(),
             g_epcStr[0] ? g_epcStr : "-", g_addrStr[0] ? g_addrStr : "-");
    g_resetReason = rich;
  } else {
    g_resetReason = pr.reason;
  }

  Serial.println("[boot] settings");
  settingsBegin();
  loadSettings(g_settings);
  sys::setLanguage(gm::languageFromCode(g_settings.clock.lang.c_str()));


  Serial.println("[boot] display");
  gfxBegin(g_settings);
  char word[48];
  gfxSplash(g_safeMode ? sys::text(sys::Recovering, word, sizeof(word)) : FW_VERSION, 5);
  // The splash fades in; from here appApplyBrightness() owns the backlight.
  g_lastBr = g_shownBr = appEffectiveBrightness();
  gfxFadeSteps(0, g_shownBr, 500, g_settings.backlightInverted);

  Serial.println("[boot] net");
  netAllowRetry(!g_safeMode);   // a retry's reboot would clear the crash flag
  netBegin(g_settings, bootProgress);
  // Arm SNTP now that WiFi (STA) is up, but only when something needs the clock
  // (clockReapply decides), so a device that shows none of it doesn't pay the
  // SNTP heap cost. Skipped after a
  // crash so a fault in here can't boot-loop before the web server starts (the
  // device then comes up in safe mode, OTA-recoverable, instead of needing UART).
  if (!g_safeMode) clockReapply(g_settings);

  Serial.println("[boot] web");
  webPortalBegin(g_settings);
  if (netMode() == NET_STA) gfxSplashUpdate(nullptr, 80);


  Serial.println("[boot] modes");
  for (size_t i = 0; i < kModeCount; i++) kModes[i]->begin(g_settings);
  Serial.println("[boot] done");
  gfxSplashUpdate(nullptr, 100);
  delay(250);

  if (netMode() == NET_AP) {
    showApInfo(true);
  } else if (g_safeMode) {
    // Last boot crashed: show the crash address (persistent) and keep the web
    // server up for OTA recovery — don't enter the render path that crashed.
    gfxCrash(g_epcStr, g_addrStr, netIP().c_str());
  } else {
    // Show which network we joined and how to reach the web UI, long enough to read.
    gfxStaInfo(netSSID().c_str(), netIP().c_str(), g_settings.hostname.c_str());
    delay(3500);
  }
}

void loop() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < g_minHeap) g_minHeap = freeHeap;
  // For the crash record (Platform.cpp): what the loop is doing, the heap low
  // and the Wi-Fi sleep type, as the callback cannot safely ask for them.
  g_minHeapSeen = g_minHeap;
  g_wifiSleep = (uint8_t)WiFi.getSleepMode();
  g_loopPhase = PHASE_NET;
  netLoop();
  g_loopPhase = PHASE_WEB;
  webPortalLoop();
  g_loopPhase = PHASE_APPS;
  // The words of the device's own screens follow a saved language change.
  sys::setLanguage(gm::languageFromCode(g_settings.clock.lang.c_str()));

#ifdef BTN_PIN
  buttonService();
  // The network-info screen (a long-press action) holds for BTN_INFO_MS, then
  // the current mode repaints over it. Handled here so it works in AP mode too.
  if (g_btnInfoUntil && millis() >= g_btnInfoUntil) {
    g_btnInfoUntil = 0;
    if (netMode() == NET_AP) {
      showApInfo(true);
    } else {
      DisplayMode* m = activeMode(g_settings);
      if (m) m->wake(g_settings);
    }
  }
#endif

  if (webPortalRebootDue()) {
    delay(120);
    gfxFadeSteps(g_shownBr, 0, 400, g_settings.backlightInverted);   // the next boot fades back in
    ESP.restart();
  }
  if (webPortalRebootPending()) {
    delay(5);
    return;   // an update's last screen (or the old one) stays until the restart
  }

  if (g_safeMode) {
    delay(5);
    return;  // crashed last boot: web UI stays up for OTA recovery, no rendering
  }


  if (netMode() == NET_AP) {
    // Setup mode: AP info stays on screen, redrawn when a background retry
    // of the saved networks fails with a new reason.
    static uint16_t shownFail = netFailSeq();
    if (netFailSeq() != shownFail) {
      shownFail = netFailSeq();
      g_apPage = 1;   // the reason is on the text page
      showApInfo(false);
    } else if (g_apPages > 1 && millis() - g_apPageAt >= kApPageMs
#ifdef BTN_PIN
               && !g_btnInfoUntil   // the button's network screen holds its time
#endif
               ) {
      g_apPage ^= 1;
      showApInfo(false);
    }
    delay(5);
    return;
  }

#ifdef BTN_PIN
  if (g_btnInfoUntil) {
    delay(5);
    return;  // network-info screen is up; don't let the mode repaint over it
  }
#endif

  // --- STA mode: the active feature fetches + renders itself ---

  // Night-mode state machine (NTP-trust gate), then apply the effective brightness
  // (night override / auto-brightness / manual level).
  clockService(g_settings);
  appApplyBrightness();
  g_timerMode.tick(g_settings);   // a timer that reaches zero raises its banner here
  g_pomodoroMode.tick(g_settings);   // so does each Pomodoro phase

  // On expiry the carousel dwell is credited back the time it was hidden, so it
  // resumes on the same feature with the same remaining slice. heldMs() spans
  // the whole run rather than the last request in it, so a queue that chained
  // four overlays credits back all four.
  static bool wasNotifying = false;
  if (g_notifyMode.active()) {
    wasNotifying = true;
    g_notifyMode.service(g_settings);
    delay(5);
    return;
  }
  bool restore = wasNotifying;
  if (wasNotifying) {
    wasNotifying = false;
    if (g_carSwitch) g_carSwitch += g_notifyMode.heldMs();
  }

  // A running timer started with "show" sits over the app, as a notice does.
  static bool wasTimer = false;
  if (g_timerMode.overlay()) {
    if (!wasTimer || restore) g_timerMode.invalidate(g_settings);
    wasTimer = true;
    g_timerMode.service(g_settings);
    delay(5);
    return;
  }
  if (wasTimer) { wasTimer = false; restore = true; }
  // Then a running Pomodoro started with "show".
  static bool wasPomodoro = false;
  if (g_pomodoroMode.overlay()) {
    if (!wasPomodoro || restore) g_pomodoroMode.invalidate(g_settings);
    wasPomodoro = true;
    g_pomodoroMode.service(g_settings);
    delay(5);
    return;
  }
  if (wasPomodoro) { wasPomodoro = false; restore = true; }

  // At night, the night clock over every app.
  static bool wasNightstand = false;
  if (nightstandWanted(g_settings)) {
    nightstandService(g_settings, !wasNightstand || restore);
    wasNightstand = true;
    delay(20);
    return;
  }
  if (wasNightstand) { wasNightstand = false; restore = true; }

  DisplayMode* m = activeMode(g_settings);
  if (m) {
    if (restore) m->wake(g_settings);
    m->service(g_settings);
  }

  delay(5);
}
