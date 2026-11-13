// Settings for the sun schedule and the device-wide location: defaults,
// limits, the JSON round trip, the move from older config files, the radar's
// "use the device location" switch, and "sun schedule or light sensor, not both".
// Built with the stand-in core headers of the fuzz target (see tests/sun/run.sh).
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "Settings.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Stand-ins the real Settings.cpp links against (as in tests/fuzz/fuzz_parsers.cpp).
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

static void testDefaults() {
  Settings s;
  s.setDefaults();
  CHECK(!s.clock.sunEnabled, "sun off by default");
  CHECK(s.clock.sunLow == 5, "lowest brightness %d", s.clock.sunLow);
  CHECK(s.clock.sunRise == 60 && s.clock.sunSet == 60, "fades %d %d", s.clock.sunRise, s.clock.sunSet);
  CHECK(s.clock.sunOverlap == 0, "overlap: night mode first");
  CHECK(s.clock.lat == 0.0f && s.clock.lon == 0.0f, "no location");
}

static void testLimits() {
  Settings s;
  s.setDefaults();
  apply(s, "{\"clock\":{\"sunEnabled\":true,\"sunLow\":0,\"sunRise\":0,\"sunSet\":120,\"sunOverlap\":2,\"lat\":52.52,\"lon\":13.41}}");
  CHECK(s.clock.sunEnabled, "enabled");
  CHECK(s.clock.sunLow == 0, "lowest brightness 0 is allowed");
  CHECK(s.clock.sunRise == 0 && s.clock.sunSet == 120, "fades 0 and 120");
  CHECK(s.clock.sunOverlap == 2, "overlap darker");
  CHECK(fabsf(s.clock.lat - 52.52f) < 1e-4f && fabsf(s.clock.lon - 13.41f) < 1e-4f, "location");
  apply(s, "{\"clock\":{\"sunLow\":250,\"sunRise\":999,\"sunSet\":-5,\"sunOverlap\":9,\"lat\":123.0,\"lon\":-400.0}}");
  CHECK(s.clock.sunLow == 100, "lowest brightness capped at 100: %d", s.clock.sunLow);
  CHECK(s.clock.sunRise == 120, "sunrise fade capped at 120: %d", s.clock.sunRise);
  CHECK(s.clock.sunSet == 0, "sunset fade floored at 0: %d", s.clock.sunSet);
  CHECK(s.clock.sunOverlap == 2, "overlap capped at 2: %d", s.clock.sunOverlap);
  CHECK(s.clock.lat == 90.0f, "latitude capped at 90: %f", s.clock.lat);
  CHECK(s.clock.lon == -180.0f, "longitude capped at -180: %f", s.clock.lon);
  // Whole numbers are locations too (a page or a script may send 45, not 45.0).
  apply(s, "{\"clock\":{\"lat\":45,\"lon\":-7}}");
  CHECK(s.clock.lat == 45.0f && s.clock.lon == -7.0f, "integer location: %f %f", s.clock.lat, s.clock.lon);
  apply(s, "{\"clock\":{\"lat\":123.0,\"lon\":-400.0}}");
  // Wrong types are ignored, the old value stays.
  apply(s, "{\"clock\":{\"sunLow\":\"x\",\"sunEnabled\":5,\"lat\":\"north\"}}");
  CHECK(s.clock.sunLow == 100 && s.clock.sunEnabled && s.clock.lat == 90.0f, "wrong types ignored");
  // A partial update leaves the other fields alone.
  apply(s, "{\"clock\":{\"sunRise\":30}}");
  CHECK(s.clock.sunRise == 30 && s.clock.sunSet == 0 && s.clock.sunLow == 100, "partial update");
}

static void testRoundTrip() {
  Settings s;
  s.setDefaults();
  apply(s, "{\"clock\":{\"sunEnabled\":true,\"sunLow\":7,\"sunRise\":45,\"sunSet\":90,\"sunOverlap\":1,\"lat\":-33.87,\"lon\":151.21}}");
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), true);
  Settings t;
  t.setDefaults();
  settingsApplyJson(t, doc.as<JsonObjectConst>());
  CHECK(t.clock.sunEnabled && t.clock.sunLow == 7 && t.clock.sunRise == 45 &&
        t.clock.sunSet == 90 && t.clock.sunOverlap == 1, "sun settings survive a round trip");
  CHECK(fabsf(t.clock.lat + 33.87f) < 1e-3f && fabsf(t.clock.lon - 151.21f) < 1e-3f, "location survives");
  // Four decimals (about 10 m) survive the whole path: apply, write the file
  // text, read it back. The sun schedule and the weather use the location.
  Settings g;
  g.setDefaults();
  apply(g, "{\"clock\":{\"lat\":-33.8688,\"lon\":151.2093}}");
  JsonDocument gd;
  settingsToJson(g, gd.to<JsonObject>(), true);
  char text[2048];
  serializeJson(gd, text, sizeof(text));
  JsonDocument gd2;
  CHECK(!deserializeJson(gd2, text), "file text parses");
  Settings g2;
  g2.setDefaults();
  settingsApplyJson(g2, gd2.as<JsonObjectConst>());
  CHECK(fabsf(g2.clock.lat + 33.8688f) < 5e-5f && fabsf(g2.clock.lon - 151.2093f) < 5e-5f,
        "device location keeps 4 decimals: %.5f %.5f", g2.clock.lat, g2.clock.lon);
  CHECK(strstr(text, "151.2093") != nullptr, "the file text carries 4 decimals: %s", text);
  // The web API output (no secrets) carries them too, for the settings page.
  JsonDocument api;
  settingsToJson(s, api.to<JsonObject>(), false);
  CHECK(api["clock"]["sunEnabled"].as<bool>() == true && api["clock"]["sunRise"].as<int>() == 45,
        "present in the API output");
}

static void testOldConfigs() {
  // A config from before this feature: nothing new in it.
  Settings s;
  s.setDefaults();
  apply(s, "{\"brightness\":40,\"clock\":{\"tz\":\"Europe/Rome\",\"nightEnabled\":true,\"nightLevel\":10}}");
  CHECK(!s.clock.sunEnabled && s.clock.sunLow == 5 && s.clock.sunRise == 60, "defaults for a config without the keys");
  CHECK(s.clock.nightEnabled && s.clock.nightLevel == 10, "older keys still read");
  // Keys of the removed radar, ticker and Home Assistant are ignored and not written back.
  Settings r;
  r.setDefaults();
  apply(r, "{\"radar\":{\"lat\":52.3676,\"lon\":4.9041,\"rangeKm\":30},\"ticker\":{\"symbols\":[{\"symbol\":\"AAPL\"}]},\"ha\":{\"brokerHost\":\"x\"}}");
  JsonDocument rd;
  settingsToJson(r, rd.to<JsonObject>(), true);
  CHECK(rd["radar"].isNull() && rd["ticker"].isNull() && rd["ha"].isNull(), "removed features are not written");
  CHECK(r.clock.lat == 0.0f, "the radar home is not the device location");
}

static void testExclusive() {
  Settings s;
  s.setDefaults();
  apply(s, "{\"autoBrightness\":true}");
  CHECK(s.autoBrightness && !s.clock.sunEnabled, "light sensor alone");
  // Turning the sun schedule on in a later save turns the sensor off.
  apply(s, "{\"clock\":{\"sunEnabled\":true}}");
  CHECK(s.clock.sunEnabled && !s.autoBrightness, "sun on turns the sensor off");
  // And the other way round.
  apply(s, "{\"autoBrightness\":true}");
  CHECK(s.autoBrightness && !s.clock.sunEnabled, "sensor on turns the sun schedule off");
  // Both in one save, as the settings page sends them: only one can be true.
  Settings t;
  t.setDefaults();
  apply(t, "{\"autoBrightness\":true,\"clock\":{\"sunEnabled\":true}}");
  CHECK(!(t.autoBrightness && t.clock.sunEnabled), "never both");
  CHECK(t.clock.sunEnabled, "sun wins when both arrive together");
  // A stored config with both (hand edited): sun wins.
  Settings u;
  u.setDefaults();
  u.autoBrightness = true;
  apply(u, "{\"clock\":{\"sunEnabled\":true,\"sunLow\":4}}");
  CHECK(u.clock.sunEnabled && !u.autoBrightness, "stored sensor switched off");
}

int main() {
  testDefaults();
  testLimits();
  testRoundTrip();
  testOldConfigs();
  testExclusive();
  printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
