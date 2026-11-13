// config.h — compile-time constants of the GeekMagic SmallTV firmware.
//
// One board: the GeekMagic SmallTV-Ultra, an ESP-12F (ESP8266) with a 1.54"
// 240x240 ST7789 IPS panel. Its pin map and panel quirks are in
// board_esp8266.h; everything else is here.
#pragma once

// ---------------------------------------------------------------------------
// Firmware identity
// ---------------------------------------------------------------------------
// The firmware is called geekmagic-<version>; the version counts up by 0.01
// with every flashed build (1.36, 1.37, ...).
#define FW_NAME     "geekmagic"
#define FW_VERSION  "1.40"
#define FW_VARIANT  "esp8266"

// Display wiring + panel quirks: TFT_SCLK/MOSI/DC/RST/CS/BL, TFT_BGR,
// TFT_BL_DEFAULT_INVERTED, HAS_LDR/LDR_PIN/ADC_MAX.
#include "board_esp8266.h"

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Panel RAM offsets per rotation pair (Arduino_GFX: offset1 -> rotation 0/1,
// offset2 -> rotation 2/3). The ST7789(V) is a 240x320 controller but the
// 1.54" glass only wires RAM rows 0-239, leaving an 80-row dead band. With
// the MADCTL MY bit set (rotation 2/3) the scan direction reverses into that
// dead band, so the row offset must jump to 80 or the image slides 80 px off
// the glass (content pushed to the top at 180°). #ifndef so a board header
// can override if a variant turns up with a 240x280 panel (would need 40).
#ifndef TFT_COL_OFFSET1
#define TFT_COL_OFFSET1 0
#endif
#ifndef TFT_ROW_OFFSET1
#define TFT_ROW_OFFSET1 0
#endif
#ifndef TFT_COL_OFFSET2
#define TFT_COL_OFFSET2 0
#endif
#ifndef TFT_ROW_OFFSET2
#define TFT_ROW_OFFSET2 80
#endif

// ---------------------------------------------------------------------------
// Limits (bound RAM usage on the ESP8266)
// ---------------------------------------------------------------------------
#define MAX_WIFI_NETS     4    // saved WiFi networks; strongest visible wins at boot

// ---------------------------------------------------------------------------
// Web UI password (off by default). Digest auth, so the password itself is
// never sent over the wire even though the UI is plain HTTP.
// ---------------------------------------------------------------------------
#define MAX_AUTH_USER_LEN 32
#define MAX_AUTH_PASS_LEN 64
#define DEFAULT_AUTH_USER "admin"
#define AUTH_REALM        "GeekMagic"

// ---------------------------------------------------------------------------
// Display mode — what the device shows (settings "mode"). 0, 1, 2 and 5 were
// the ticker, the Claude usage meter, the plane radar and Home Assistant,
// removed in earlier versions.
// ---------------------------------------------------------------------------
#define MODE_CAROUSEL  3             // rotate through the ticked apps on a timer
#define MODE_NOTIFY    4             // transient overlay: armed over HTTP, never persisted
#define MODE_THEME     6             // declarative installed clock faces
#define MODE_CALENDAR  7             // the Calendar app: this month as a grid
#define MODE_REMINDERS 8             // the Reminders app: a to-do list sent from a phone
#define MODE_WEATHER   9             // the Weather app: Open-Meteo at the device location
#define MODE_TIMER     10            // the Timer app: one countdown
#define DEFAULT_MODE MODE_THEME
#define DEFAULT_CAROUSEL_SEC 30      // per-mode dwell in carousel

// ---------------------------------------------------------------------------
// Physical button (boards that define BTN_PIN). Short and long press each run
// a configurable action, and the touch sensitivity is adjustable in the web UI.
// The values here are the first-boot defaults.
// ---------------------------------------------------------------------------
#define BTN_ACT_NONE       0   // do nothing
#define BTN_ACT_SCREEN_OFF 1   // blink the backlight off for ~1 s
#define BTN_ACT_HOME       2   // jump to the first page and resume the carousel
#define BTN_ACT_NEXT       3   // next page, and pause the carousel auto-rotation
#define BTN_ACT_NETINFO    4   // show the network / IP screen briefly
#define BTN_ACT_WAKE       5   // wake the screen to normal brightness briefly (e.g. from night mode)
#define BTN_ACT_ACK        6   // acknowledge the active notification (expire it now)
#define BTN_ACT_MAX        BTN_ACT_ACK

#define BTN_LONG_MS        600 // press held >= this many ms is a long press
#define BTN_INFO_MS       4000 // how long the network-info screen stays up
#define BTN_WAKE_MS       5000 // how long a wake press keeps the screen lit
#define BTN_CONFIRM          3 // consecutive agreeing samples required to flip state

#ifdef BTN_TOUCH_RATIO
#define DEFAULT_BTN_SENS   BTN_TOUCH_RATIO  // press when touchRead < baseline * sens/100
#else
#define DEFAULT_BTN_SENS   85
#endif
#define DEFAULT_BTN_SHORT  BTN_ACT_NEXT
#define DEFAULT_BTN_LONG   BTN_ACT_NETINFO

// Full-screen attention overlay (POST /api/notify), in seconds.
#define NOTIFY_TTL_DEFAULT_SEC  20
#define NOTIFY_TTL_MIN_SEC       2
#define NOTIFY_TTL_MAX_SEC     120

// Pending overlays wait in a fixed-size queue rather than overwriting each other,
// so a burst shows every event in turn instead of only the last one. Depth and
// label length are the whole RAM cost of the feature and the ESP8266 pays for
// them out of the same 80 KB arena as the heap, so it gets the smaller pair:
// 2 x 64 B of label against 4 x 112 B here.
#define NOTIFY_QUEUE_DEPTH       2
#define NOTIFY_LABEL_MAX        48
#define NOTIFY_TITLE_MAX        20   // the word above the label, one panel line

// Priority decides queue order and pre-emption: a strictly higher one jumps the
// queue and takes the panel from whatever is on it. Values are what the API's
// "priority" field accepts, so they are part of the contract.
#define NOTIFY_PRIO_LOW          0
#define NOTIFY_PRIO_MED          1
#define NOTIFY_PRIO_HIGH         2
#define NOTIFY_PRIO_MAX          3

// Marquee for labels wider than the panel: pixels per step and step interval.
// A character is GFX_FONT_W * NOTIFY_LABEL_SIZE = 12 px wide, so 4 px every
// 40 ms is 100 px/s, or 120 ms per character. At that speed a 90-character
// label (1108 px including the gap) takes about 11 s for one pass, so the
// default 20 s hold shows it just under twice. Half this speed did not finish a
// single pass within the default hold. The step is what changed, not the
// interval: the band is redrawn at the same 25 Hz as before.
#define NOTIFY_SCROLL_STEP_PX    4
#define NOTIFY_SCROLL_MS        40
#define NOTIFY_SCROLL_GAP_PX    28   // blank run between the end and the repeat
#define NOTIFY_SCROLL_PAUSE_MS 900   // hold at the start before scrolling begins

// ---------------------------------------------------------------------------
// Defaults (used on first boot / factory reset)
// ---------------------------------------------------------------------------
#define DEFAULT_AP_SSID      "GeekMagic-Setup"
#define DEFAULT_AP_PASS      ""              // empty => open AP
#define DEFAULT_HOSTNAME     "geekmagic"
#define DEFAULT_BRIGHTNESS    90             // 0..100 %
#define DEFAULT_HTTP_TIMEOUT  8000           // ms per request

// --- Panel colour correction (device-wide) ---
// Panels differ between (and within) the SmallTV variants: white balance drifts
// and some controllers have red and blue swapped. AUTO keeps the board header's
// TFT_BGR default; RGB/BGR force the MADCTL colour-order bit either way.
#define COLOR_ORDER_AUTO   0
#define COLOR_ORDER_RGB    1
#define COLOR_ORDER_BGR    2
#define DEFAULT_COLOR_ORDER  COLOR_ORDER_AUTO
#define DEFAULT_COLOR_INVERT false
#define DEFAULT_COLOR_GAIN   100     // percent per channel; 50..150 accepted
#define MIN_COLOR_GAIN        50
#define MAX_COLOR_GAIN       150

// --- Clock / night mode (device-wide) ---
#define NTP_SERVER1             "pool.ntp.org"
#define NTP_SERVER2             "time.nist.gov"
#define DEFAULT_TZ_NAME         ""        // IANA display name; empty = UTC
#define DEFAULT_TZ_POSIX        "UTC0"    // POSIX TZ rule the device feeds SNTP
#define DEFAULT_NIGHT_ENABLED   false
#define DEFAULT_NIGHT_START_MIN 1320      // 22:00
#define DEFAULT_NIGHT_END_MIN   420       // 07:00
#define DEFAULT_NIGHT_LEVEL     0         // 0..100, 0 = backlight fully off

// Night-mode NTP trust: only ENTER night mode when the clock was confirmed by a
// successful NTP sync within NIGHT_NTP_TRUST_MS (else we assume the clock may be
// wrong and keep the screen on). While inside the window but unconfirmed, re-arm
// SNTP every NIGHT_NTP_RESYNC_MS until a fresh sync lands or the window ends
// (morning). Once night mode has switched on, it stays on until the window ends.
#define NIGHT_NTP_TRUST_MS      300000UL  // 5 min: max age of the sync that unlocks night
#define NIGHT_NTP_RESYNC_MS      30000UL  // re-sync attempt cadence while held off
