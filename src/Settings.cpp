#include "Settings.h"
#include "AtomicJson.h"
#include "Platform.h"   // platformChipId() for the unique default hostname
#include "features/theme/Languages.h"
#include <LittleFS.h>

static const char* CONFIG_PATH = "/config.json";
// Bytes in CONFIG_PATH as last read or written: what the next save needs on
// the filesystem, known without building the settings document again.
static size_t g_storedSize = 0;

// ===========================================================================
// Clock / night mode slice
// ===========================================================================
static uint16_t hhmmToMin(const char* s, uint16_t fallback) {
  if (!s || !s[0]) return fallback;
  int h = 0, m = 0;
  if (sscanf(s, "%d:%d", &h, &m) != 2) return fallback;
  if (h < 0 || h > 23 || m < 0 || m > 59) return fallback;
  return (uint16_t)(h * 60 + m);
}
static String minToHhmm(uint16_t v) {
  if (v > 1439) v = 0;
  char b[16];
  snprintf(b, sizeof(b), "%02u:%02u", (unsigned)(v / 60), (unsigned)(v % 60));
  return String(b);
}

void ClockSettings::setDefaults() {
  tz            = DEFAULT_TZ_NAME;
  tzPosix       = DEFAULT_TZ_POSIX;
  lang          = "en";
  nightEnabled  = DEFAULT_NIGHT_ENABLED;
  nightStartMin = DEFAULT_NIGHT_START_MIN;
  nightEndMin   = DEFAULT_NIGHT_END_MIN;
  nightLevel    = DEFAULT_NIGHT_LEVEL;
  lat = lon     = 0;
  sunEnabled    = false;
  sunLow        = 5;
  sunRise = sunSet = 60;
  sunOverlap    = 0;
  nightstand    = false;
}

void ClockSettings::toJson(JsonObject o) const {
  o["tz"]           = tz;
  o["tzPosix"]      = tzPosix;
  o["lang"]         = lang;
  o["nightEnabled"] = nightEnabled;
  o["nightStart"]   = minToHhmm(nightStartMin);
  o["nightEnd"]     = minToHhmm(nightEndMin);
  o["nightLevel"]   = nightLevel;
  o["lat"]          = lat;
  o["lon"]          = lon;
  o["sunEnabled"]   = sunEnabled;
  o["sunLow"]       = sunLow;
  o["sunRise"]      = sunRise;
  o["sunSet"]       = sunSet;
  o["sunOverlap"]   = sunOverlap;
  o["nightstand"]   = nightstand;
}

void ClockSettings::fromJson(JsonObjectConst o) {
  if (o["tz"].is<const char*>())          tz = o["tz"].as<String>();
  if (o["tzPosix"].is<const char*>())     tzPosix = o["tzPosix"].as<String>();
  if (o["lang"].is<const char*>()) {
    const char* code = o["lang"];
    if (gm::languageIndex(code, strlen(code)) >= 0) lang = code;   // an unknown code keeps the old one
  }
  if (o["nightEnabled"].is<bool>())       nightEnabled = o["nightEnabled"];
  if (o["nightStart"].is<const char*>())  nightStartMin = hhmmToMin(o["nightStart"], nightStartMin);
  if (o["nightEnd"].is<const char*>())    nightEndMin   = hhmmToMin(o["nightEnd"], nightEndMin);
  if (o["nightLevel"].is<int>())          nightLevel = constrain((int)o["nightLevel"], 0, 100);
  if (o["lat"].is<float>() || o["lat"].is<int>()) lat = constrain(o["lat"].as<float>(), -90.0f, 90.0f);
  if (o["lon"].is<float>() || o["lon"].is<int>()) lon = constrain(o["lon"].as<float>(), -180.0f, 180.0f);
  if (o["sunEnabled"].is<bool>())         sunEnabled = o["sunEnabled"];
  if (o["sunLow"].is<int>())              sunLow = constrain((int)o["sunLow"], 0, 100);
  if (o["sunRise"].is<int>())             sunRise = constrain((int)o["sunRise"], 0, 120);
  if (o["sunSet"].is<int>())              sunSet = constrain((int)o["sunSet"], 0, 120);
  if (o["sunOverlap"].is<int>())          sunOverlap = constrain((int)o["sunOverlap"], 0, 2);
  if (o["nightstand"].is<bool>())         nightstand = o["nightstand"];
}

// ===========================================================================
// Physical button slice
// ===========================================================================
void ButtonSettings::setDefaults() {
  sensitivity = DEFAULT_BTN_SENS;
  shortAction = DEFAULT_BTN_SHORT;
  longAction  = DEFAULT_BTN_LONG;
}

void ButtonSettings::toJson(JsonObject o) const {
  o["sensitivity"] = sensitivity;
  o["shortAction"] = shortAction;
  o["longAction"]  = longAction;
}

void ButtonSettings::fromJson(JsonObjectConst o) {
  if (o["sensitivity"].is<int>()) sensitivity = constrain((int)o["sensitivity"], 50, 99);
  if (o["shortAction"].is<int>()) shortAction = constrain((int)o["shortAction"], 0, BTN_ACT_MAX);
  if (o["longAction"].is<int>())  longAction  = constrain((int)o["longAction"],  0, BTN_ACT_MAX);
}

// ===========================================================================
// Web UI password slice
// ===========================================================================
void AuthSettings::setDefaults() {
  user = DEFAULT_AUTH_USER;
  pass = "";
}

void AuthSettings::toJson(JsonObject o, bool includeSecrets) const {
  // "enabled" is no setting any more: it mirrors the password. It is still
  // written (and sent to the page) so that older firmware, which reads it, and
  // older pages treat a locked device as locked.
  o["enabled"] = pass.length() > 0;
  o["user"]    = user;
  o["passSet"] = pass.length() > 0;
  if (includeSecrets) o["pass"] = pass;
}

// One parser for the stored file, a save from the page and an import.
// A stored password is what locks the page. Rules:
//  - "removePass": true takes the password off. It is the only way to do so;
//    a blank "pass" keeps the stored one, as everywhere else in this file.
//  - "enabled": false next to a "pass" means that password was never in force
//    (older firmware let you store one with the box unticked). It is ignored,
//    so an update does not start locking a device whose owner may have
//    forgotten it. On its own, "enabled" (true or false) changes nothing: an
//    old page or script cannot unlock or lock the device with it.
//  - A password with no "enabled" key counts (a hand-edited file).
void AuthSettings::fromJson(JsonObjectConst o) {
  if (o["user"].is<const char*>()) user = o["user"].as<String>();
  if (o["removePass"].is<bool>() && o["removePass"].as<bool>()) pass = "";
  bool neverInForce = o["enabled"].is<bool>() && !o["enabled"].as<bool>();
  if (!neverInForce && o["pass"].is<const char*>()) {
    String p = o["pass"].as<String>();
    if (p.length()) pass = p;
  }
  if (user.length() >= MAX_AUTH_USER_LEN) user.remove(MAX_AUTH_USER_LEN - 1);
  if (pass.length() >= MAX_AUTH_PASS_LEN) pass.remove(MAX_AUTH_PASS_LEN - 1);
  if (!user.length()) user = DEFAULT_AUTH_USER;
}

// ===========================================================================
// Panel colour slice
// ===========================================================================
void DisplaySettings::setDefaults() {
  colorOrder = DEFAULT_COLOR_ORDER;
  invert     = DEFAULT_COLOR_INVERT;
  rGain = gGain = bGain = DEFAULT_COLOR_GAIN;
  notifySec = 0;
}

void DisplaySettings::toJson(JsonObject o) const {
  o["colorOrder"] = (colorOrder == COLOR_ORDER_RGB) ? "rgb"
                  : (colorOrder == COLOR_ORDER_BGR) ? "bgr" : "auto";
  o["invert"] = invert;
  o["rGain"]  = rGain;
  o["gGain"]  = gGain;
  o["bGain"]  = bGain;
  o["notifySec"] = notifySec;
}

void DisplaySettings::fromJson(JsonObjectConst o) {
  if (o["colorOrder"].is<const char*>()) {
    String c = o["colorOrder"].as<String>();
    colorOrder = c.equalsIgnoreCase("rgb") ? COLOR_ORDER_RGB
               : c.equalsIgnoreCase("bgr") ? COLOR_ORDER_BGR : COLOR_ORDER_AUTO;
  }
  if (o["invert"].is<bool>()) invert = o["invert"];
  if (o["rGain"].is<int>()) rGain = constrain((int)o["rGain"], MIN_COLOR_GAIN, MAX_COLOR_GAIN);
  if (o["gGain"].is<int>()) gGain = constrain((int)o["gGain"], MIN_COLOR_GAIN, MAX_COLOR_GAIN);
  if (o["bGain"].is<int>()) bGain = constrain((int)o["bGain"], MIN_COLOR_GAIN, MAX_COLOR_GAIN);
  if (o["notifySec"].is<int>()) {
    const int sec = o["notifySec"];
    notifySec = sec <= 0 ? 0 : constrain(sec, NOTIFY_TTL_MIN_SEC, NOTIFY_TTL_MAX_SEC);
  }
}

// ===========================================================================
// Top-level settings
// ===========================================================================
void Settings::setDefaults() {
  wifiCount = 0;
  for (uint8_t i = 0; i < MAX_WIFI_NETS; i++) {
    wifi[i].ssid = "";
    wifi[i].pass = "";
  }
  apSsid  = DEFAULT_AP_SSID;
  apPass  = DEFAULT_AP_PASS;
  // Unique per device so several SmallTVs on one network don't collide on
  // mDNS out of the box. A hostname saved in config.json overrides this.
  hostname = String(DEFAULT_HOSTNAME) + "-" + String(platformChipId() & 0xFFFF, HEX);

  mode = DEFAULT_MODE;
  themeId = "modular";   // built into the image, installed on first start (ThemeMode)
  carouselSec = DEFAULT_CAROUSEL_SEC;
  carouselTheme = false;   // opt in: needs an installed theme, else the slot shows an error
  carouselCalendar = false;
  carouselReminders = false;
  carouselWeather = false;
  carouselTimer = false;
  httpTimeout = DEFAULT_HTTP_TIMEOUT;

  brightness = DEFAULT_BRIGHTNESS;
  autoBrightness = false;
  backlightInverted = TFT_BL_DEFAULT_INVERTED;
  rotation = 0;

  clock.setDefaults();
  display.setDefaults();
  auth.setDefaults();
  button.setDefaults();
}

// ---------------------------------------------------------------------------
bool settingsBegin() {
  if (LittleFS.begin()) return true;
  // First boot on a fresh chip: format then mount.
  if (LittleFS.format() && LittleFS.begin()) return true;
  return false;
}

bool loadSettings(Settings& s) {
  s.setDefaults();
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  g_storedSize = f.size();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  settingsApplyJson(s, doc.as<JsonObjectConst>());
  return true;
}

bool saveSettings(const Settings& s) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  settingsToJson(s, root, /*includeSecrets=*/true);

  // Never truncate the working settings when a full disk or interrupted
  // save prevents writing the replacement.
  if (!saveJsonAtomically(LittleFS, doc, CONFIG_PATH, "/config.json.tmp")) return false;
  g_storedSize = measureJson(doc);   // saveJsonAtomically() wrote exactly this
  return true;
}

void factoryReset(Settings& s) {
  LittleFS.remove(CONFIG_PATH);
  g_storedSize = 0;
  s.setDefaults();
}

size_t settingsStoredSize() { return g_storedSize; }

// ---------------------------------------------------------------------------
void settingsToJson(const Settings& s, JsonObject root, bool includeSecrets) {
  root["hostname"]   = s.hostname;

  // WiFi networks. Passwords only reach the config file, never the web API.
  JsonArray wf = root["wifi"].to<JsonArray>();
  for (uint8_t i = 0; i < s.wifiCount; i++) {
    JsonObject e = wf.add<JsonObject>();
    e["ssid"]    = s.wifi[i].ssid;
    e["passSet"] = s.wifi[i].pass.length() > 0;
    if (includeSecrets) e["pass"] = s.wifi[i].pass;
  }
  // Legacy mirror of the primary network, kept for one release so a firmware
  // downgrade still finds its WiFi in config.json.
  root["staSsid"]    = s.wifiCount ? s.wifi[0].ssid : "";
  root["staPassSet"] = s.wifiCount && s.wifi[0].pass.length() > 0;
  root["apSsid"]     = s.apSsid;
  root["apPassSet"]  = s.apPass.length() > 0;
  if (includeSecrets) {
    root["staPass"]  = s.wifiCount ? s.wifi[0].pass : "";
    root["apPass"]   = s.apPass;
  }

  // Mode + shared HTTP/display
  root["themeId"] = s.themeId;
  root["mode"]              = (s.mode == MODE_THEME)    ? "theme"
                            : (s.mode == MODE_CALENDAR) ? "calendar"
                            : (s.mode == MODE_REMINDERS) ? "reminders"
                            : (s.mode == MODE_WEATHER) ? "weather"
                            : (s.mode == MODE_TIMER) ? "timer"
                            : (s.mode == MODE_CAROUSEL) ? "carousel" : "theme";
  root["carouselSec"]       = s.carouselSec;
  root["carouselTheme"]     = s.carouselTheme;
  root["carouselCalendar"]  = s.carouselCalendar;
  root["carouselReminders"] = s.carouselReminders;
  root["carouselWeather"]   = s.carouselWeather;
  root["carouselTimer"]     = s.carouselTimer;
  root["httpTimeout"]       = s.httpTimeout;
  root["brightness"]        = s.brightness;
  root["autoBrightness"]    = s.autoBrightness;
  root["backlightInverted"] = s.backlightInverted;
  root["rotation"]          = s.rotation;

  // Feature slices
  s.clock.toJson(root["clock"].to<JsonObject>());
  s.display.toJson(root["display"].to<JsonObject>());
  s.auth.toJson(root["auth"].to<JsonObject>(), includeSecrets);
  s.button.toJson(root["button"].to<JsonObject>());
}

// Apply only the keys that are present (partial update friendly). Accepts both
// the nested layout and the legacy flat layout (feature keys at the top level).
void settingsApplyJson(Settings& s, JsonObjectConst root) {
  if (root["hostname"].is<const char*>()) s.hostname = root["hostname"].as<String>();

  if (root["wifi"].is<JsonArrayConst>()) {
    // The list is authoritative when present (order = try priority, missing
    // row = deletion). A blank password keeps the stored one, matched by SSID
    // so rows survive reordering, unless the row says "open": true (joining
    // a network that has become open must not reuse its old password).
    WifiCred old[MAX_WIFI_NETS];
    uint8_t oldCount = s.wifiCount;
    for (uint8_t i = 0; i < oldCount; i++) old[i] = s.wifi[i];

    s.wifiCount = 0;
    for (JsonObjectConst e : root["wifi"].as<JsonArrayConst>()) {
      if (s.wifiCount >= MAX_WIFI_NETS) break;
      const char* ssid = e["ssid"] | "";
      if (!ssid[0]) continue;                // skip blank rows
      WifiCred& dst = s.wifi[s.wifiCount];
      dst.ssid = ssid;
      const char* pass = e["pass"] | "";
      dst.pass = pass;
      if (!pass[0] && !(e["open"] | false))
        for (uint8_t i = 0; i < oldCount; i++)
          if (old[i].ssid == dst.ssid) { dst.pass = old[i].pass; break; }
      s.wifiCount++;
    }
  } else if (root["staSsid"].is<const char*>()) {
    // Legacy single-network layout (pre-2.4 config.json or an old cached web
    // page): it becomes/updates the primary network, extras stay untouched.
    String ssid = root["staSsid"].as<String>();
    if (ssid.length()) {
      s.wifi[0].ssid = ssid;
      if (root["staPass"].is<const char*>()) {
        String p = root["staPass"].as<String>();
        if (p.length() > 0) s.wifi[0].pass = p;   // blank = keep
      }
      if (s.wifiCount < 1) s.wifiCount = 1;
    }
  }
  if (root["apSsid"].is<const char*>()) s.apSsid = root["apSsid"].as<String>();
  // AP password: apply as-is when present (empty allowed => open AP).
  if (root["apPass"].is<const char*>()) s.apPass = root["apPass"].as<String>();

  if (root["mode"].is<const char*>()) {
    String m = root["mode"].as<String>();
    s.mode = m.equalsIgnoreCase("theme")    ? MODE_THEME
           : m.equalsIgnoreCase("calendar") ? MODE_CALENDAR
           : m.equalsIgnoreCase("reminders") ? MODE_REMINDERS
           : m.equalsIgnoreCase("weather") ? MODE_WEATHER
           : m.equalsIgnoreCase("timer") ? MODE_TIMER
           : m.equalsIgnoreCase("carousel") ? MODE_CAROUSEL
           // apps this firmware no longer has: the watch face instead
           : (m.equalsIgnoreCase("stocks") || m.equalsIgnoreCase("radar") || m.equalsIgnoreCase("ha") ||
              m.equalsIgnoreCase("usage")) ? MODE_THEME
           : s.mode;   // empty or unknown: a save that names no mode keeps the stored one
  }
  if (root["themeId"].is<const char*>()) {
    String id = root["themeId"].as<String>();
    bool valid = id.length() <= 48;
    for (size_t i = 0; i < id.length(); ++i) {
      char c = id[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) valid = false;
    }
    if (valid) s.themeId = id;
  }
  if (root["carouselSec"].is<int>())      s.carouselSec = constrain((int)root["carouselSec"], 5, 3600);
  if (root["carouselTheme"].is<bool>())   s.carouselTheme = root["carouselTheme"];
  if (root["carouselCalendar"].is<bool>()) s.carouselCalendar = root["carouselCalendar"];
  if (root["carouselReminders"].is<bool>()) s.carouselReminders = root["carouselReminders"];
  if (root["carouselWeather"].is<bool>()) s.carouselWeather = root["carouselWeather"];
  if (root["carouselTimer"].is<bool>()) s.carouselTimer = root["carouselTimer"];

  if (root["httpTimeout"].is<int>())        s.httpTimeout = constrain((int)root["httpTimeout"], 1000, 20000);
  if (root["brightness"].is<int>())         s.brightness = constrain((int)root["brightness"], 0, 100);
  if (root["autoBrightness"].is<bool>())    s.autoBrightness = root["autoBrightness"];
  if (root["backlightInverted"].is<bool>()) s.backlightInverted = root["backlightInverted"];
  if (root["rotation"].is<int>())           s.rotation = (uint8_t)(((int)root["rotation"]) & 3);

  // Feature slices, each only when its object is present.
  if (root["clock"].is<JsonObjectConst>()) s.clock.fromJson(root["clock"].as<JsonObjectConst>());
  if (root["display"].is<JsonObjectConst>()) s.display.fromJson(root["display"].as<JsonObjectConst>());
  if (root["auth"].is<JsonObjectConst>()) s.auth.fromJson(root["auth"].as<JsonObjectConst>());
  if (root["button"].is<JsonObjectConst>()) s.button.fromJson(root["button"].as<JsonObjectConst>());

  // The sun schedule and the light sensor each pick the brightness alone: not
  // both. Whichever this save turned on wins; if both arrive on (or both were
  // already stored), the sun schedule wins.
  if (s.autoBrightness && s.clock.sunEnabled) {
    JsonObjectConst c = root["clock"].as<JsonObjectConst>();
    bool autoNew = root["autoBrightness"].is<bool>() && root["autoBrightness"].as<bool>();
    bool sunNew = c["sunEnabled"].is<bool>() && c["sunEnabled"].as<bool>();
    if (autoNew && !sunNew) s.clock.sunEnabled = false;
    else s.autoBrightness = false;
  }
}
