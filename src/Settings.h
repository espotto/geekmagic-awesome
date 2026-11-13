// Settings.h — persisted configuration (LittleFS /config.json)
//
// Layout is segmented per feature: shared device/network fields live at the top
// level, and each feature owns a nested settings slice (clock, display, ...).
// config.json mirrors this: { ..shared.., "clock":{...}, "display":{...} }.
// Keys of removed features (ticker, radar, ha, usage, wg) in an older file are
// ignored and dropped by the next save.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// One saved WiFi station network. The device keeps up to MAX_WIFI_NETS and
// joins the strongest visible one at boot (hidden SSIDs are tried last).
struct WifiCred {
  String ssid;
  String pass;
};

// ---- Clock / night mode slice (device-wide) --------------------------------
struct ClockSettings {
  String   tz;            // IANA display name, e.g. "Europe/Rome" (UI round-trip)
  String   tzPosix;       // POSIX TZ rule the device feeds SNTP
  String   lang;          // device language: en or ru; the clock's screens and the web page follow it
  bool     nightEnabled;  // dim/blank on a nightly schedule
  uint16_t nightStartMin; // minutes since local midnight (0..1439)
  uint16_t nightEndMin;
  uint8_t  nightLevel;    // 0..100, 0 = backlight off
  // Device-wide location (0,0 = not set): the sun schedule and the weather.
  float    lat, lon;
  // Brightness that follows sunrise and sunset (see SunTimes.h)
  bool     sunEnabled;
  uint8_t  sunLow;        // 0..100 %, brightness at night (0 = off)
  uint8_t  sunRise;       // sunrise fade, 0..120 minutes
  uint8_t  sunSet;        // sunset fade, 0..120 minutes
  uint8_t  sunOverlap;    // with night mode: 0 night first, 1 sun first, 2 darker
  bool     nightstand;    // at night, a large dim clock over every app

  void setDefaults();
  void toJson(JsonObject o) const;
  void fromJson(JsonObjectConst o);
};

// ---- Web UI password slice (device-wide) -----------------------------------
// Off by default: the settings page is open on the LAN, as it always was.
// A stored password is what locks it: every page and every API endpoint is then
// behind HTTP digest auth, with the exceptions listed in WebPortal.cpp. There is
// no separate on/off switch. Like the other secrets here, the password reaches
// the config file and the settings export, never the web API.
struct AuthSettings {
  String user;
  String pass;

  void setDefaults();
  void toJson(JsonObject o, bool includeSecrets) const;
  void fromJson(JsonObjectConst o);
};

// ---- Panel colour slice (device-wide) --------------------------------------
// Same firmware, different panels: the SmallTV variants and even units of one
// variant render the same RGB565 value differently, and a few have red and blue
// swapped in the controller. These settings correct that per device.
struct DisplaySettings {
  uint8_t colorOrder;   // COLOR_ORDER_AUTO / _RGB / _BGR (auto = board default)
  bool    invert;       // flip the panel's inversion bit (washed-out / negative panels)
  uint8_t rGain;        // per-channel gain in percent, 50..150, 100 = untouched
  uint8_t gGain;
  uint8_t bGain;
  // How long a notification stays up, in seconds: the apps' own banners
  // (Timer, Pomodoro) and every /api/notify request without its own "ttl".
  // 0 keeps each type's own hold time (20-45 s), otherwise
  // NOTIFY_TTL_MIN_SEC..NOTIFY_TTL_MAX_SEC for every type.
  uint8_t notifySec;

  void setDefaults();
  void toJson(JsonObject o) const;
  void fromJson(JsonObjectConst o);
};

// ---- Physical button slice (device-wide) -----------------------------------
// Only acted on where BTN_PIN exists, but the fields persist everywhere so a
// config export/import round-trips across boards. sensitivity is the touch press
// threshold as a percent of the idle baseline; shortAction/longAction are BTN_ACT_*.
struct ButtonSettings {
  uint8_t sensitivity;   // press when touchRead < baseline * sensitivity/100 (50..99)
  uint8_t shortAction;   // BTN_ACT_*
  uint8_t longAction;    // BTN_ACT_*

  void setDefaults();
  void toJson(JsonObject o) const;
  void fromJson(JsonObjectConst o);
};

// ---- Pomodoro slice (the app's durations) ----------------------------------
struct PomodoroSettings {
  uint8_t focusMin;   // 1..120
  uint8_t shortMin;   // 1..60
  uint8_t longMin;    // 1..60
  uint8_t rounds;     // focus sessions before the long break, 1..8
  bool    autoNext;   // the next phase starts by itself

  void setDefaults();
  void toJson(JsonObject o) const;
  void fromJson(JsonObjectConst o);
};

// ---- Top-level settings ----------------------------------------------------
struct Settings {
  // --- WiFi station networks (the device joins one of these) ---
  WifiCred wifi[MAX_WIFI_NETS];
  uint8_t  wifiCount;

  // --- Access point (config / fallback hotspot) ---
  String apSsid;
  String apPass;        // empty => open network
  String hostname;      // mDNS name => http://<hostname>.local

  // --- Active feature ---
  String themeId;       // installed .stheme ID; empty until selected
  uint8_t mode;         // MODE_THEME, MODE_CALENDAR, ..., MODE_CAROUSEL

  // --- Carousel (mode == MODE_CAROUSEL): dwell + which features rotate ---
  uint16_t carouselSec;
  bool carouselTheme, carouselCalendar, carouselReminders, carouselWeather, carouselTimer, carouselPomodoro;

  // --- Shared HTTP / display ---
  uint16_t httpTimeout; // ms
  uint8_t  brightness;        // 0..100 %
  bool     autoBrightness;    // use LDR on A0
  bool     backlightInverted; // active-low backlight
  uint8_t  rotation;          // 0..3 screen orientation

  // --- Feature slices ---
  ClockSettings   clock;
  DisplaySettings display;   // panel colour correction
  AuthSettings    auth;      // optional web UI password
  ButtonSettings  button;    // physical button behaviour (boards with BTN_PIN)
  PomodoroSettings pomodoro; // the Pomodoro app's durations

  void setDefaults();
};

// Persistence
bool settingsBegin();                       // mount LittleFS
bool loadSettings(Settings& s);             // false => defaults applied
bool saveSettings(const Settings& s);
void factoryReset(Settings& s);             // wipe file + defaults
size_t settingsStoredSize();                // bytes of the last load or save

// JSON <-> struct. `includeSecrets=false` masks passwords for the web API.
void settingsToJson(const Settings& s, JsonObject root, bool includeSecrets);
void settingsApplyJson(Settings& s, JsonObjectConst root); // partial update allowed
