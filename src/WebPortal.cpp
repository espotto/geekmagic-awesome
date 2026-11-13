#include "WebPortal.h"
#include "SysText.h"
#include "DefaultFaces.h"
#include "Platform.h"
#include "WebGuard.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "webui.h"
#include "PwaAssets.h"
#include "RemindersMode.h"
#include "WeatherMode.h"
#include "TimerMode.h"
#include "StatusJson.h"
#include "Net.h"
#include "Gfx.h"
#include "NotifyMode.h"
#include "Clock.h"
#include "features/theme/ThemeWeb.h"
#include "features/theme/SystemFont.h"
#include "SettingsTransaction.h"

// Defined in main.cpp — re-init every mode + force a repaint after a config change.
extern void appInvalidate();
extern void appRelease();
extern const char* appResetReason();   // last reset reason (diagnostics)
extern void appApplyBrightness();   // main.cpp: re-resolve effective brightness now
extern int  appBacklightSteps();    // main.cpp: backlight level now, 0..255
extern uint32_t appMinHeap();       // main.cpp: lowest free heap seen since boot

static WebServerClass server(80);
static Settings*        S = nullptr;
static bool             g_reboot = false;
static uint32_t         g_rebootAt = 0;

static void scheduleReboot(uint32_t inMs) {
  g_reboot = true;
  g_rebootAt = millis() + inMs;
}

// ---- CSRF and DNS-rebinding guard ------------------------------------------
// Every handler goes through requireAuth(), which calls this first. The two upload callbacks (OTA, theme install) run
// before their final handler, so they only ask guardAllowed() and write nothing
// when it says no; the final handler then answers 403 through requireAuth().
// Rules in WebGuard.h.
//   - Host: checked on every request, but not in AP mode. There the phone's
//     captive-portal probe arrives with whatever name it is checking
//     (connectivitycheck.gstatic.com, captive.apple.com, ...) and must get the
//     redirect; the network is the device's own, so nothing can rebind to it.
//   - Origin / Sec-Fetch-Site: checked on anything that is not GET/HEAD/OPTIONS.
//     A request carrying neither (curl, Home Assistant, n8n, the daemon) passes.
// guardReject() returns the refusal text, or nullptr when the request may
// proceed. It answers nothing, so it is safe inside an upload callback.
static const char* guardReject() {
  String host = server.hostHeader();
  if (netMode() != NET_AP) {
    const char* name = S->hostname.length() ? S->hostname.c_str() : DEFAULT_HOSTNAME;
    if (!webguard::hostAllowed(host.c_str(), name)) return "Forbidden: unexpected Host header";
  }
  HTTPMethod m = server.method();
  if (m != HTTP_GET && m != HTTP_HEAD && m != HTTP_OPTIONS) {
    String origin = server.header("Origin");
    String site   = server.header("Sec-Fetch-Site");
    if (!webguard::originAllowed(origin.c_str(), site.c_str(), host.c_str()))
      return "Forbidden: cross-site request";
  }
  return nullptr;
}

static bool guardAllowed() { return guardReject() == nullptr; }

// Returns true when the request may proceed; on false it has already answered.
static bool guardRequest() {
  const char* why = guardReject();
  if (!why) return true;
  server.send(403, "text/plain", why);
  return false;
}

// ---- optional web UI password ---------------------------------------------
// Off by default. While a password is stored, every handler below starts with
// this line and every page and endpoint needs the credentials. Digest, not basic, so the password
// is never sent over a plain-HTTP LAN. One deliberate exception: the
// captive-portal probes, which a phone fires before anyone can type a password
// and which only ever redirect.
// Returns true when the request may proceed; on false it has already answered.
static bool requireAuth() {
  if (!guardRequest()) return false;
  if (!S->auth.pass.length()) return true;   // a stored password is the lock
  if (server.authenticate(S->auth.user.c_str(), S->auth.pass.c_str())) return true;
  server.requestAuthentication(HTTPAuthMethod::DIGEST_AUTH, AUTH_REALM,
                               "Authentication required");
  return false;
}

// ---------------------------------------------------------------------------
static void sendJson(JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void handleRoot() {
  if (!requireAuth()) return;
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Content-Encoding", "gzip");   // WEBUI_HTML_GZ is gzip'd (webui.h is generated)
  server.send_P(200, "text/html", (PGM_P)WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
}

// The web app manifest and icons, so the panel can be added to a phone's home
// screen and open as an app of its own. Open without the password: a phone
// fetches them without credentials, and they say nothing about the device.
static void sendAsset(const char* type, const uint8_t* data, size_t len) {
  server.sendHeader("Cache-Control", "public, max-age=604800");
  server.send_P(200, type, (PGM_P)data, len);
}
static void handleManifest() { sendAsset("application/manifest+json", PWA_MANIFEST, PWA_MANIFEST_LEN); }
static void handleAppleIcon() { sendAsset("image/png", PWA_APPLE_ICON, PWA_APPLE_ICON_LEN); }
static void handleIcon192()   { sendAsset("image/png", PWA_ICON_192, PWA_ICON_192_LEN); }
static void handleIcon512()   { sendAsset("image/png", PWA_ICON_512, PWA_ICON_512_LEN); }

static void handleGetConfig() {
  if (!requireAuth()) return;
  // The full settings need about 4 KB of heap to send, which a running theme
  // on the ESP8266 does not leave free; the theme reloads on its next frame.
  appRelease();
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  settingsToJson(*S, root, /*includeSecrets=*/false);
  // Physical button: only some boards have one, so the UI hides its card otherwise.
  JsonObject feat = root["features"].to<JsonObject>();
  JsonArray builtIn = root["builtInFaces"].to<JsonArray>();   // the page offers no Remove for these
  for (const DefaultFace& face : DEFAULT_FACES) builtIn.add(face.id);
#ifdef BTN_PIN
  feat["button"] = true;
#else
  feat["button"] = false;
#endif
  // Which chip this build runs on (the UI warns about per-chip limitations).
  root["chip"] = "esp8266";
  sendJson(doc);
}

// The pieces of the status reply, to the client as they come, with a pause
// after each (CONTRIBUTING: a loop over input-sized data yields).
struct StatusSink : gm::Sink {
  bool write(const char* data, size_t count) override {
    server.sendContent(data, count);
    yield();
    return true;
  }
};


// GET /api/status. The reply is written in 128-byte pieces as the values are
// read, never built whole (StatusJson.h): its length is not known before the
// last value, and uptime and heap change meanwhile, so it goes out chunked.
// The original handler built a document and a String of it; six requests at once then left
// the ESP8266 with a 648-byte largest block, and a diag build crashed.
static void handleStatus() {
  if (!requireAuth()) return;
  const String ssid = netSSID(), ip = netIP();
  const String clockText = clockTimeStr();
  gm::StatusData d;
  d.fw = FW_NAME;
  d.version = FW_VERSION;
  d.variant = FW_VARIANT;   // the chip, "esp8266"
  d.mode = (netMode() == NET_AP) ? "ap" : "sta";
  d.connected = netConnected();
  d.ssid = ssid.c_str();
  d.ip = ip.c_str();
  d.rssi = netRSSI();
  d.wifiFail = netFailReason();
  d.heap = ESP.getFreeHeap();
  d.maxblk = platformMaxFreeBlock();     // largest contiguous block (TLS handshake needs one)
  d.contstk = platformFreeContStack();   // primary stack headroom (ESP8266)
  d.minHeap = appMinHeap();
  NetHealth nh;
  netHealth(nh);
  d.hasNet = netMode() == NET_STA;
  d.drops = nh.drops;
  d.lastDrop = nh.lastDrop;
  d.gateway = nh.gateway;
  d.silentFor = nh.silentFor;
  d.rejoins = nh.rejoins;
  d.lastRestart = nh.lastRestart;
  d.uptime = millis() / 1000;
  d.reset = appResetReason();
  d.synced = clockSynced();
  d.time = clockText.c_str();
  d.tz = S->clock.tz.c_str();
  d.night = clockNightActive();          // dimming now
  d.nightHeld = clockNightHeld();        // in the window but waiting for a fresh NTP sync
  d.clockFresh = clockTrusted();         // last NTP sync within the trust window
  char sunText[24] = "";
  if (!clockSunText(*S, sunText, sizeof(sunText))) sunText[0] = 0;
  d.sun = sunText;                       // today's sunrise-sunset; left out when empty
  d.bl = appBacklightSteps();            // backlight level now, 0..255

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  StatusSink sink;
  gm::PieceWriter<128> out(sink);
  gm::writeStatusHead(out, d);
  PlatformCrashRecord cr;   // last crash, kept in RTC memory until cleared or power-cycled
  if (platformCrashRecordGet(cr)) {
    char cause[12], epc[12], addr[12];
    snprintf(cause, sizeof cause, "0x%x", (unsigned)cr.exccause);
    snprintf(epc, sizeof epc, "0x%08x", (unsigned)cr.epc1);
    snprintf(addr, sizeof addr, "0x%08x", (unsigned)cr.excvaddr);
    gm::StatusCrash k;
    k.reason = cr.reason;
    k.fw = cr.fw;
    if (cr.hasRegisters) { k.exccause = cause; k.epc = epc; k.addr = addr; }
    k.hasUptime = cr.uptimeKnown;
    k.uptime = cr.uptimeS;
    gm::writeStatusCrash(out, k);
  }
  gm::writeStatusEnd(out);
  server.sendContent("", 0);   // the last chunk
}

// Fingerprint of everything network-identity related: the WiFi list and the
// hostname. Changing any of it needs a reboot, because the connection and the
// mDNS registration are established once at boot.
static String netFingerprint(const Settings& s) {
  String f((int)s.wifiCount);
  for (uint8_t i = 0; i < s.wifiCount; i++) {
    f += '\n';
    f += s.wifi[i].ssid;
    f += '\x01';
    f += s.wifi[i].pass;
  }
  f += '\n';
  f += s.hostname;
  return f;
}

// A settings body that could not be parsed. Running out of heap is reported
// as that: on the ESP8266 it is the usual cause, and "bad json" sent people
// looking for a syntax error that was not there.
static void sendParseError(DeserializationError err) {
  if (err == DeserializationError::NoMemory)
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"not enough memory\"}");
  else
    server.send(400, "application/json", String("{\"ok\":false,\"error\":\"bad json (") + err.c_str() + ")\"}");
}

static void handlePostConfig() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "no body"); return; }

  // Parsing the page's full settings and building the document for the file
  // need about 7 KB of heap. On the ESP8266 that is only free with the
  // running theme released; appInvalidate() below brings it back.
  appRelease();
  JsonDocument doc;
  if (DeserializationError err = deserializeJson(doc, server.arg("plain"))) {
    sendParseError(err);
    return;
  }

  String oldNet = netFingerprint(*S);

  // The request is freed once applied, before saveSettings() builds its own
  // document: the two together do not fit on the ESP8266.
  if (!applyAndSaveSettings(*S,
        [&](Settings& settings) { settingsApplyJson(settings, doc.as<JsonObjectConst>()); doc.clear(); },
        [](const Settings& settings) { return saveSettings(settings); })) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"could not save settings\"}");
    return;
  }

  // Live apply (no reboot needed for these)
  clockReapply(*S);         // re-arm SNTP iff the timezone changed
  appApplyBrightness();     // apply effective brightness (respects night/auto/manual)
  gfxApplyColors(*S);       // rotation, panel colour order/inversion, channel gain
  appInvalidate();          // re-init every mode + repaint (covers mode/URL/symbol changes)

  bool wifiChanged = netFingerprint(*S) != oldNet;

  JsonDocument res;
  res["ok"] = true;
  res["reboot"] = wifiChanged;
  sendJson(res);

  if (wifiChanged) scheduleReboot(800);
}

static void handleScan() {
  if (!requireAuth()) return;
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 25; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"] = !platformScanIsOpen(i);
  }
  WiFi.scanDelete();
  sendJson(doc);
}

// GET /api/font: the system font (~45 KB, straight from flash) for the web UI,
// which draws theme previews in the browser. It changes only with the
// firmware, so the page asks for it with the version in the query and the
// browser keeps it.
struct FontStream {
  char buf[512];
  size_t used = 0;
  void flush() {
    if (used) server.sendContent(buf, used);
    used = 0;
  }
};
static void fontChunk(void* context, const uint8_t* data, size_t length) {
  FontStream& out = *static_cast<FontStream*>(context);
  while (length) {
    const size_t room = sizeof(out.buf) - out.used, n = length < room ? length : room;
    memcpy(out.buf + out.used, data, n);
    out.used += n;
    data += n;
    length -= n;
    if (out.used == sizeof(out.buf)) out.flush();
  }
}
static void handleFont() {
  if (!requireAuth()) return;
  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.setContentLength(gm::writeSystemFont(nullptr, nullptr));
  server.send(200, "application/octet-stream", "");
  FontStream out;
  gm::writeSystemFont(fontChunk, &out);
  out.flush();
}

static void handleReboot() {
  if (!requireAuth()) return;
  server.send(200, "application/json", "{\"ok\":true}");
  scheduleReboot(400);
}

static void handleCrashClear() {
  if (!requireAuth()) return;
  platformCrashRecordClear();
  server.send(200, "application/json", "{\"ok\":true}");
}

// Erase All Content and Settings: the settings, the reminders, the watch faces
// the user added and the crash record. The built-in faces stay. The restart
// then starts like a new device: the setup hotspot, Modular on the screen.
static void handleFactory() {
  if (!requireAuth()) return;
  factoryReset(*S);
  saveSettings(*S);
  LittleFS.remove("/reminders.json");
  LittleFS.remove("/reminders.tmp");
  Dir dir = LittleFS.openDir("/themes");
  while (dir.next()) {
    if (!dir.isFile()) continue;
    bool builtIn = false;
    for (const DefaultFace& face : DEFAULT_FACES) builtIn |= dir.fileName() == String(face.id) + ".stheme";
    if (!builtIn) LittleFS.remove("/themes/" + dir.fileName());   // the index too: rebuilt as needed
  }
  platformCrashRecordClear();
  server.send(200, "application/json", "{\"ok\":true}");
  scheduleReboot(400);
}

// Full settings backup: stream the persisted config.json verbatim. It includes
// the WiFi passwords — same trust domain as typing them into this page.
static void handleExport() {
  if (!requireAuth()) return;
  File f = LittleFS.open("/config.json", "r");
  if (!f) { server.send(404, "text/plain", "no config saved yet"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=geekmagic-config.json");
  server.streamFile(f, "application/json");
  f.close();
}

// Restore a backup: apply everything, persist, reboot (WiFi/hostname may change).
static void handleImport() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "no body"); return; }
  appRelease();   // as in handlePostConfig(); the reboot below restores it
  JsonDocument doc;
  if (DeserializationError err = deserializeJson(doc, server.arg("plain"))) {
    sendParseError(err);
    return;
  }
  if (!applyAndSaveSettings(*S,
        [&](Settings& settings) { settingsApplyJson(settings, doc.as<JsonObjectConst>()); doc.clear(); },
        [](const Settings& settings) { return saveSettings(settings); })) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"could not save settings\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  scheduleReboot(800);
}



// Attention overlay. Every field is optional and falls back to the preset named
// by "type" (itself defaulting to "info"), so the smallest useful request is
// {"label":"..."} and the largest overrides the lot:
//   {"type":"alert","title":"NAS","label":"<text>","anim":"bang",
//    "color":"#3399ff","ttl":<seconds>,"priority":0..3}
// Never persisted. Behind the password like the rest of the API: taking over
// the whole screen is not something to leave open on a device you locked.
static void handleNotify() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "no body"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }
  // "state" is what the endpoint shipped with, before types existed. It named
  // the same two things "type" now does, so it is read as an alias rather than
  // a second field: scripts written against the old API keep working untouched.
  const char* type = doc["type"];
  if (!type) type = doc["state"];

  NotifyRequest r;
  r.type  = type;
  r.title = doc["title"];
  r.label = doc["label"];
  r.anim  = doc["anim"];
  r.color = doc["color"];
  r.ttlSec = doc["ttl"] | (uint32_t)0;              // 0 -> the Display setting, else the preset's hold
  if (!r.ttlSec && S) r.ttlSec = S->display.notifySec;
  r.prio   = doc["priority"] | (int)-1;             // <0 -> the preset's own priority

  // A full queue answers 429, not 400: it is the one failure worth retrying, and
  // a script cannot back off sensibly if it looks the same as a typo in "type".
  switch (g_notifyMode.request(r)) {
    case NOTIFY_REJECTED:
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"bad request\"}");
      return;
    case NOTIFY_QUEUE_FULL:
      server.send(429, "application/json", "{\"ok\":false,\"error\":\"queue full\"}");
      return;
    case NOTIFY_ACCEPTED:
      break;
  }

  char body[48];
  snprintf(body, sizeof(body), "{\"ok\":true,\"queued\":%u}",
           (unsigned)g_notifyMode.queued());
  server.send(200, "application/json", body);
}

// ---- Reminders ------------------------------------------------------------
// GET lists the items; POST changes them (see RemindersMode::apply) and
// answers the new list, so a client never needs a second request.
static void sendReminders(int code, const String& error) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  if (code == 200) g_remindersMode.toJson(root);
  else { root["ok"] = false; root["error"] = error; }
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}
static void handleRemindersGet() {
  if (!requireAuth()) return;
  sendReminders(200, String());
}
static void handleRemindersPost() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "no body"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
  String error;
  const int code = g_remindersMode.apply(doc.as<JsonVariantConst>(), error);
  sendReminders(code, error);
}

// ---- Weather and Timer -----------------------------------------------------
static void sendObject(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}
// GET: the last reading; POST {"refresh":true}: fetch again on the next pass.
static void handleWeather() {
  if (!requireAuth()) return;
  if (server.method() == HTTP_POST) g_weatherMode.refresh();
  JsonDocument doc;
  g_weatherMode.toJson(doc.to<JsonObject>());
  sendObject(200, doc);
}
// GET: the timer; POST: change it (TimerMode::apply), answered with the new state.
static void handleTimer() {
  if (!requireAuth()) return;
  JsonDocument doc;
  int code = 200;
  if (server.method() == HTTP_POST) {
    JsonDocument body;
    if (!server.hasArg("plain") || deserializeJson(body, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
    String error;
    code = g_timerMode.apply(body.as<JsonVariantConst>(), error);
    if (code != 200) { doc["ok"] = false; doc["error"] = error; sendObject(code, doc); return; }
  }
  g_timerMode.toJson(doc.to<JsonObject>());
  sendObject(code, doc);
}

// ---- OTA ------------------------------------------------------------------
static void handleUpdateDone() {
  if (!requireAuth()) return;
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : platformUpdateError().c_str());
  if (ok) scheduleReboot(1200);
  else appInvalidate();   // the update screen gives way to the feature again
}

// The upload callback runs while the body streams in, ahead of
// handleUpdateDone, so the password has to be checked here too: guarding only
// the final handler would let an unauthenticated POST write a whole image to
// flash and merely lose the 200 at the end.
static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (!guardAllowed()) {
    if (up.status != UPLOAD_FILE_START) Update.end();
    return;
  }
  if (S->auth.pass.length() &&
      !server.authenticate(S->auth.user.c_str(), S->auth.pass.c_str())) {
    if (up.status != UPLOAD_FILE_START) Update.end();
    return;
  }
  if (up.status == UPLOAD_FILE_START) {
    WiFiUDP::stopAll();   // free UDP sockets so the OTA has max contiguous flash/heap
    uint32_t maxSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSpace)) Update.printError(Serial);
    { char w[48]; gfxSplash(sys::text(sys::Updating, w, sizeof(w)), 0); }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    // The request's length includes the multipart framing, so the bar ends a
    // touch short of full; UPLOAD_FILE_END fills it.
    if (up.contentLength) gfxSplashUpdate(nullptr, (int)((uint64_t)up.totalSize * 100 / up.contentLength));
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) Update.printError(Serial);
    else { char w[48]; gfxSplashUpdate(sys::text(sys::Restarting, w, sizeof(w)), 100); }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.end();
  }
  yield();
}

// ---- captive portal -------------------------------------------------------
static void handleNotFound() {
  if (netMode() == NET_AP) {
    // Redirect everything to the config page so the captive portal pops.
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

// ---------------------------------------------------------------------------
void webPortalBegin(Settings& settings) {
  S = &settings;

  themeWebBegin(server, settings, requireAuth, guardAllowed);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/manifest.webmanifest", HTTP_GET, handleManifest);
  server.on("/api/weather", HTTP_GET, handleWeather);
  server.on("/api/weather", HTTP_POST, handleWeather);
  server.on("/api/timer", HTTP_GET, handleTimer);
  server.on("/api/timer", HTTP_POST, handleTimer);
  server.on("/api/reminders", HTTP_GET, handleRemindersGet);
  server.on("/api/reminders", HTTP_POST, handleRemindersPost);
  server.on("/apple-touch-icon.png", HTTP_GET, handleAppleIcon);
  server.on("/icon-192.png", HTTP_GET, handleIcon192);
  server.on("/icon-512.png", HTTP_GET, handleIcon512);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/font", HTTP_GET, handleFont);         // system font for theme previews
  server.on("/api/factory", HTTP_POST, handleFactory);
  server.on("/api/crash/clear", HTTP_POST, handleCrashClear);
  server.on("/api/export", HTTP_GET, handleExport);
  server.on("/api/import", HTTP_POST, handleImport);
  server.on("/api/notify", HTTP_POST, handleNotify);     // full-screen attention overlay
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);

  // Common captive-portal probe endpoints
  server.on("/generate_204", handleNotFound);
  server.on("/gen_204", handleNotFound);
  server.on("/hotspot-detect.html", handleNotFound);
  server.on("/connecttest.txt", handleNotFound);
  server.onNotFound(handleNotFound);

  // Headers the guard reads; the server drops all others.
  server.collectHeaders("Origin", "Sec-Fetch-Site");   // variadic, keeps Authorization

  server.begin();
}

void webPortalLoop() {
  server.handleClient();
}

bool webPortalRebootPending() { return g_reboot; }

bool webPortalRebootDue() {
  return g_reboot && (int32_t)(millis() - g_rebootAt) >= 0;
}
