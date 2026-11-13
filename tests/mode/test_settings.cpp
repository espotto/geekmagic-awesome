// The "mode" key of a save or an import. A save that does not name a mode the
// build knows must leave the stored mode alone. Before, an empty or unknown
// text turned the mode into the stock ticker. Built with the stand-in core
// headers of the fuzz target (see tests/mode/run.sh).
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "Settings.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

FSClass LittleFS;
EspClass ESP;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    g_checks++;                                                            \
    if (!(cond)) {                                                         \
      g_failed++;                                                          \
      printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);               \
      printf(__VA_ARGS__);                                                 \
      printf("\n");                                                        \
    }                                                                      \
  } while (0)

static void apply(Settings& s, const char* json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) { printf("bad test json: %s\n", json); g_failed++; return; }
  settingsApplyJson(s, doc.as<JsonObjectConst>());
}

// Start from a device that stored `mode`, then apply a save.
static uint8_t after(uint8_t stored, const char* json) {
  Settings s;
  s.setDefaults();
  s.mode = stored;
  apply(s, json);
  return s.mode;
}

static void testNamesSetTheMode() {
  CHECK(after(MODE_WEATHER, "{\"mode\":\"theme\"}") == MODE_THEME, "theme");
  CHECK(after(MODE_THEME, "{\"mode\":\"calendar\"}") == MODE_CALENDAR, "calendar");
  CHECK(after(MODE_THEME, "{\"mode\":\"weather\"}") == MODE_WEATHER, "weather");
  CHECK(after(MODE_THEME, "{\"mode\":\"carousel\"}") == MODE_CAROUSEL, "carousel");
  CHECK(after(MODE_WEATHER, "{\"mode\":\"Theme\"}") == MODE_THEME, "case is ignored");
  // Apps this firmware no longer has (ticker, radar, Home Assistant, Clawdmeter): the watch face.
  CHECK(after(MODE_WEATHER, "{\"mode\":\"stocks\"}") == MODE_THEME, "stocks");
  CHECK(after(MODE_WEATHER, "{\"mode\":\"radar\"}") == MODE_THEME, "radar");
  CHECK(after(MODE_WEATHER, "{\"mode\":\"ha\"}") == MODE_THEME, "ha");
  CHECK(after(MODE_WEATHER, "{\"mode\":\"usage\"}") == MODE_THEME, "usage");
}

static void testEmptyMode() {
  // The page sends "" when its mode list lacks the stored mode (a build
  // without that feature). The stored mode stays.
  CHECK(after(MODE_WEATHER, "{\"mode\":\"\"}") == MODE_WEATHER, "weather kept");
  CHECK(after(MODE_THEME, "{\"mode\":\"\"}") == MODE_THEME, "theme kept");
  CHECK(after(MODE_CAROUSEL, "{\"mode\":\"\"}") == MODE_CAROUSEL, "carousel kept");
}

static void testUnknownMode() {
  CHECK(after(MODE_WEATHER, "{\"mode\":\"nonsense\"}") == MODE_WEATHER, "unknown text kept");
  CHECK(after(MODE_WEATHER, "{\"mode\":\"theme2\"}") == MODE_WEATHER, "near miss kept");
}

static void testNoModeOrOtherType() {
  CHECK(after(MODE_CALENDAR, "{\"brightness\":40}") == MODE_CALENDAR, "no mode key");
  CHECK(after(MODE_CALENDAR, "{\"mode\":7}") == MODE_CALENDAR, "number");
  CHECK(after(MODE_CALENDAR, "{\"mode\":null}") == MODE_CALENDAR, "null");
}

static void testImport() {
  // An import goes through the same parser, onto the stored settings: an
  // export from a lean build with an empty mode does not move the mode.
  Settings s;
  s.setDefaults();
  CHECK(s.mode == MODE_THEME, "the watch face by default");
  apply(s, "{\"mode\":\"weather\",\"hostname\":\"geekmagic-test\"}");
  CHECK(s.mode == MODE_WEATHER, "stored weather");
  apply(s, "{\"mode\":\"\",\"hostname\":\"geekmagic-test-2\"}");
  CHECK(s.mode == MODE_WEATHER, "import with empty mode keeps weather");
  CHECK(s.hostname == "geekmagic-test-2", "the rest of the import applies");
}

// A saved network's password: a blank one keeps it, "open": true clears it
// (joining a network that has become open), and SSIDs keep their spaces.
static void wifiPasswords() {
  Settings s;
  s.setDefaults();
  apply(s, R"({"wifi":[{"ssid":"Home Net ","pass":"secret-1"}]})");
  CHECK(s.wifiCount == 1 && s.wifi[0].ssid == "Home Net " && s.wifi[0].pass == "secret-1", "saved");
  apply(s, R"({"wifi":[{"ssid":"Home Net "}]})");
  CHECK(s.wifi[0].pass == "secret-1", "blank keeps the stored password");
  apply(s, R"({"wifi":[{"ssid":"Home Net ","open":true}]})");
  CHECK(s.wifi[0].pass.length() == 0, "open clears it");
}

int main() {
  wifiPasswords();
  testNamesSetTheMode();
  testEmptyMode();
  testUnknownMode();
  testNoModeOrOtherType();
  testImport();
  printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
