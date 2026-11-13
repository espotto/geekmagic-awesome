#include "Net.h"
#include "Platform.h"
#include "SysText.h"
#include <DNSServer.h>
extern "C" {
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/prot/ip4.h>
}

static NetMode     g_mode = NET_AP;
static DNSServer   g_dns;
static String      g_hostname;
static String      g_apSsid;
static uint32_t    g_lastReconnect = 0;
static const Settings* g_cfg = nullptr;  // for runtime failover between saved networks
static int8_t      g_curNet = -1;        // settings index of the joined network
static uint32_t    g_downSince = 0;      // 0 = connected; else millis() the drop began

// Why the last join failed. The driver reports a disconnect reason code from
// its event task; the boot loop and the setup-mode retry turn it into a short
// text for the setup screen and /api/status. Reasons 8 (ASSOC_LEAVE) and 36
// (STA_LEAVING, ESP-IDF 5) are our own WiFi.disconnect() or the one inside
// WiFi.begin(), and would otherwise overwrite the real cause.
static volatile uint16_t g_lastReason = 0;
static volatile bool     g_assoc = false;   // associated, maybe still waiting for DHCP
static volatile uint32_t g_drops = 0;       // disconnects after a join, since boot
static volatile uint16_t g_dropReason = 0;  // the driver's reason for the last one
static String      g_failText;
static uint16_t    g_failSeq = 0;           // bumped on every new failure text

// Setup-mode retry: while the AP is up and nobody is on it, try the saved
// networks again in the background (AP+STA), one at a time.
static const uint32_t kJoinBudgetMs   = 30000;  // per network the scan can see
static const uint32_t kUnseenBudgetMs = 8000;   // per hidden / out-of-range network
static const uint32_t kRetryEveryMs   = 90000;  // setup mode: pause between attempts
static uint32_t    g_retryAt = 0;           // millis() of the last attempt's end
static uint32_t    g_attemptStart = 0;      // 0 = no setup-mode attempt running
static bool        g_retryAllowed = true;   // off in safe mode, see netAllowRetry()
// Whether the boot scan saw each network. A retry gets the same budget the boot
// join gave it: a longer one would join here, reboot, fail the shorter boot
// join again, and cycle like that forever.
static bool        g_seen[MAX_WIFI_NETS];

static void readWatchdogRestart();   // the network watchdog, below netBegin()

static void onStaDisconnected(uint16_t reason) {
  if (g_mode == NET_STA && g_curNet >= 0 && reason != 8 && reason != 36 && (g_assoc || WiFi.isConnected())) {
    g_drops++;
    g_dropReason = reason;
  }
  g_assoc = false;
  if (reason != 8 && reason != 36) g_lastReason = reason;
}

static WiFiEventHandler g_evConnected, g_evDisconnected;   // must stay alive
static void netHookEvents() {
  if (g_evDisconnected) return;
  g_evConnected = WiFi.onStationModeConnected(
      [](const WiFiEventStationModeConnected&) { g_assoc = true; });
  g_evDisconnected = WiFi.onStationModeDisconnected(
      [](const WiFiEventStationModeDisconnected& e) { onStaDisconnected((uint16_t)e.reason); });
}

// Reason codes shared by the ESP8266 SDK and ESP-IDF (200+ are Espressif's own),
// as words of the device language (SysText).
static sys::Word reasonWord(uint16_t r) {
  switch (r) {
    case 2:                             return sys::AuthTimedOut;
    case 15: case 202: case 204:        return sys::WrongPassword;
    case 201:                           return sys::NetworkNotFound;
    case 210: case 211:                 return sys::SecurityMode;
    case 203:                           return sys::RouterRefused;
    case 200:                           return sys::SignalLost;
    default:                            return sys::JoinFailed;
  }
}

// Call once an attempt has timed out, before WiFi.disconnect().
static void recordFailure(const String& ssid) {
  char why[48];
  sys::text(g_assoc ? sys::NoIp : (g_lastReason ? reasonWord(g_lastReason) : sys::TimedOut), why, sizeof(why));
  char buf[112];
  if (g_lastReason && !g_assoc)
    snprintf_P(buf, sizeof(buf), PSTR("%s: %s (%u)"), ssid.c_str(), why, (unsigned)g_lastReason);
  else
    snprintf_P(buf, sizeof(buf), PSTR("%s: %s"), ssid.c_str(), why);
  g_failText = buf;
  g_failSeq++;
  Serial.print(F("[net] join failed, "));
  Serial.println(buf);
}

static void beginJoin(const WifiCred& n) {
  g_lastReason = 0;
  g_assoc = false;
  // Put the PHY mode and TX power back to the SDK defaults before every join.
  // The SDK keeps its RF settings in flash, so after an install over the stock
  // OTA (no erase_flash) whatever the stock firmware left there stays in force.
  // A SmallTV-ultra was refused by a FRITZ! mesh with reason 203 until this and
  // the boot scan in netBegin() went in together (not tested apart). Setting
  // the defaults changes nothing for a device that already uses them.
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.setOutputPower(20.5f);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);   // see netBegin()
  WiFi.begin(n.ssid.c_str(), n.pass.c_str());
}

static void startAP(const Settings& s) {
  g_mode = NET_AP;
  // With saved networks keep the station side on, so netLoop can retry them.
  // netLoop drives every attempt itself: the driver's own reconnect would keep
  // the radio scanning off the AP channel between attempts, and on the ESP32
  // could restart a join right after netLoop ended it.
  WiFi.setAutoReconnect(false);
  WiFi.mode(s.wifiCount ? WIFI_AP_STA : WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if (s.apPass.length() >= 8) {
    WiFi.softAP(s.apSsid.c_str(), s.apPass.c_str());
  } else {
    WiFi.softAP(s.apSsid.c_str());           // open AP (WPA2 needs >=8 chars)
  }
  g_apSsid = s.apSsid;
  // Captive portal: answer every DNS query with our own IP.
  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", apIP);
  g_retryAt = millis();
}

// A word for the start-up progress line (one at a time is shown).
static const char* sysWord(sys::Word w) {
  static char buf[48];
  return sys::text(w, buf, sizeof(buf));
}

void netBegin(const Settings& s, void (*onProgress)(const char*, uint8_t)) {
  g_cfg = &s;
  g_hostname = s.hostname.length() ? s.hostname : String(DEFAULT_HOSTNAME);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  readWatchdogRestart();
  // No modem sleep: dozing between beacons saves nothing worth having on a
  // USB-powered clock, and some routers drop a dozing client without telling
  // it, which leaves the device on screen but off the network.
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  platformSetHostname(g_hostname.c_str());
  netHookEvents();

  if (s.wifiCount == 0) {
    if (onProgress) onProgress(sysWord(sys::StartingSetup), 60);
    startAP(s);
    return;
  }

  WiFi.mode(WIFI_STA);

  // Try order: scan once (blocking is fine here, only the boot screen is up)
  // and put the networks the scan can see first, strongest first. Unseen ones
  // (hidden SSIDs or currently out of range) go last, in config order, with a
  // shorter timeout each.
  uint8_t order[MAX_WIFI_NETS];
  bool    seen[MAX_WIFI_NETS];
  if (s.wifiCount == 1) {
    // Scan anyway, results unused (about 2 s at boot). Part of the fix for the
    // 203 refusal described in beginJoin(); the two went in together.
    if (onProgress) onProgress(sysWord(sys::LookingForWifi), 20);
    WiFi.scanNetworks();
    WiFi.scanDelete();
    order[0] = 0;
    seen[0] = true;
  } else {
    int32_t rssi[MAX_WIFI_NETS];
    for (uint8_t i = 0; i < s.wifiCount; i++) { rssi[i] = -32768; seen[i] = false; }
    if (onProgress) onProgress(sysWord(sys::LookingForWifi), 20);
    int found = WiFi.scanNetworks();
    for (int a = 0; a < found; a++)
      for (uint8_t i = 0; i < s.wifiCount; i++)
        if (WiFi.SSID(a) == s.wifi[i].ssid && WiFi.RSSI(a) > rssi[i]) {
          rssi[i] = WiFi.RSSI(a);
          seen[i] = true;
        }
    WiFi.scanDelete();

    bool used[MAX_WIFI_NETS] = {false};
    for (uint8_t k = 0; k < s.wifiCount; k++) {
      int best = -1;
      for (uint8_t i = 0; i < s.wifiCount; i++) {
        if (used[i]) continue;
        if (best < 0 ||
            (seen[i] && !seen[best]) ||
            (seen[i] == seen[best] && rssi[i] > rssi[best])) best = i;
      }
      used[best] = true;
      order[k] = (uint8_t)best;
    }
  }

  for (uint8_t i = 0; i < s.wifiCount; i++) g_seen[i] = seen[i];

  for (uint8_t k = 0; k < s.wifiCount; k++) {
    const WifiCred& n = s.wifi[order[k]];
    if (onProgress) {
      char fmt[40], msg[96];
      sys::text(sys::Joining, fmt, sizeof(fmt));
      snprintf(msg, sizeof(msg), fmt, n.ssid.c_str());
      onProgress(msg, (uint8_t)(35 + 20 * k / s.wifiCount));
    }
    beginJoin(n);

    uint32_t budget = seen[order[k]] ? kJoinBudgetMs : kUnseenBudgetMs;
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < budget) {
      delay(200);
      yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
      g_failText = "";
      g_curNet = (int8_t)order[k];
      g_mode = NET_STA;
      if (MDNS.begin(g_hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
      }
      if (onProgress) onProgress(sysWord(sys::Connected), 65);
      return;
    }
    recordFailure(n.ssid);
    WiFi.disconnect();
    delay(100);
  }

  if (onProgress) onProgress(sysWord(sys::StartingSetup), 60);
  startAP(s);
}

// ---- network watchdog (ESP8266) -------------------------------------------
// A station can stay "connected" to the driver while the router no longer
// hears it (with modem sleep on, some routers drop a dozing client that way):
// nothing reconnects it, and the device answers nobody until it is powered
// off. So once the router has answered one ping, it is pinged every 30 s; 4
// misses in a row (2 min) rejoin the network, 10 (5 min) restart the device.
// A router that never answers pings leaves the watchdog off.
static const uint32_t kProbeEveryMs = 30000;
static const uint8_t  kRejoinAfter  = 4;
static const uint8_t  kRestartAfter = 10;
static const uint32_t kDownRestartMs = 600000;   // link down this long: restart
static int8_t   g_gw = -1;               // 1 answering, 0 silent now, -1 never answered
static uint8_t  g_silent = 0;            // probes in a row without an answer
static uint32_t g_rejoins = 0;
static uint32_t g_probeAt = 0;
static volatile bool g_probeOut = false, g_probeGot = false;
static char g_lastRestart[40] = "";

// Remembered across the restart in RTC user memory (the crash record uses
// words 96-127): a magic word and a code.
static const uint32_t kRtcWord = 90, kRtcMagic = 0x4E455457;   // "NETW"
enum : uint32_t { WD_GATEWAY = 1, WD_DOWN = 2 };
static void watchdogRestart(uint32_t code) {
  uint32_t r[2] = {kRtcMagic, code};
  ESP.rtcUserMemoryWrite(kRtcWord, r, sizeof(r));
  Serial.printf("[net] watchdog restart (%u)\n", (unsigned)code);
  delay(100);
  ESP.restart();
}
static void readWatchdogRestart() {
  uint32_t r[2] = {0, 0};
  if (ESP.rtcUserMemoryRead(kRtcWord, r, sizeof(r)) && r[0] == kRtcMagic) {
    strcpy_P(g_lastRestart, r[1] == WD_DOWN ? PSTR("Wi-Fi down for 10 minutes")
                                            : PSTR("router silent for 5 minutes"));
    uint32_t zero[2] = {0, 0};
    ESP.rtcUserMemoryWrite(kRtcWord, zero, sizeof(zero));
  }
}

static raw_pcb* g_pingPcb = nullptr;
static uint16_t g_pingSeq = 0;
static const uint16_t kPingId = 0x474D;   // "GM"

// Runs in the network stack: takes only our own echo replies.
static u8_t pingRecv(void*, raw_pcb*, pbuf* p, const ip_addr_t*) {
  if (p->len < IP_HLEN) return 0;
  const ip_hdr* iph = static_cast<const ip_hdr*>(p->payload);
  const u16_t hl = IPH_HL(iph) * 4;
  if (p->len < hl + sizeof(icmp_echo_hdr)) return 0;
  const icmp_echo_hdr* e = reinterpret_cast<const icmp_echo_hdr*>(static_cast<const uint8_t*>(p->payload) + hl);
  if (ICMPH_TYPE(e) != ICMP_ER || e->id != PP_HTONS(kPingId) || e->seqno != lwip_htons(g_pingSeq)) return 0;
  g_probeGot = true;
  pbuf_free(p);
  return 1;
}

static bool pingGateway() {
  const IPAddress gw = WiFi.gatewayIP();
  if (!gw.isSet()) return false;
  if (!g_pingPcb) {
    g_pingPcb = raw_new(IP_PROTO_ICMP);
    if (!g_pingPcb) return false;
    raw_recv(g_pingPcb, pingRecv, nullptr);
    raw_bind(g_pingPcb, IP_ADDR_ANY);
  }
  const u16_t len = sizeof(icmp_echo_hdr) + 8;
  pbuf* p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
  if (!p) return false;
  icmp_echo_hdr* e = static_cast<icmp_echo_hdr*>(p->payload);
  ICMPH_TYPE_SET(e, ICMP_ECHO);
  ICMPH_CODE_SET(e, 0);
  e->id = PP_HTONS(kPingId);
  e->seqno = lwip_htons(++g_pingSeq);
  memset(reinterpret_cast<uint8_t*>(e) + sizeof(icmp_echo_hdr), 'g', 8);
  e->chksum = 0;
  e->chksum = inet_chksum(e, len);
  ip_addr_t to;
  ip_addr_set_ip4_u32(&to, (uint32_t)gw);
  const err_t err = raw_sendto(g_pingPcb, p, &to);
  pbuf_free(p);
  return err == ERR_OK;
}

// Called while the station is connected.
static void gatewayWatch() {
  if (g_probeAt && millis() - g_probeAt < kProbeEveryMs) return;
  if (g_probeOut) {   // the previous probe's verdict
    if (g_probeGot) { g_gw = 1; g_silent = 0; }
    else if (g_gw != -1) {
      g_gw = 0;
      if (g_silent < 255) g_silent++;
      if (g_silent >= kRestartAfter) watchdogRestart(WD_GATEWAY);
      if (g_silent == kRejoinAfter && g_cfg && g_curNet >= 0) {
        Serial.println(F("[net] router silent, rejoining"));
        g_rejoins++;
        WiFi.disconnect();
        beginJoin(g_cfg->wifi[g_curNet]);
        g_probeAt = millis();
        g_probeOut = false;
        return;
      }
    }
  }
  g_probeAt = millis();
  if (!g_probeAt) g_probeAt = 1;
  g_probeGot = false;
  g_probeOut = pingGateway();
}

void netHealth(NetHealth& out) {
  out = NetHealth();
  out.drops = g_drops;
  if (g_dropReason) sys::textIn(0, reasonWord(g_dropReason), out.lastDrop, sizeof(out.lastDrop));   // diagnostics: English
  out.gateway = g_gw;
  out.silentFor = g_silent;
  out.rejoins = g_rejoins;
  strncpy(out.lastRestart, g_lastRestart, sizeof(out.lastRestart) - 1);
}

void netLoop() {
  if (g_mode == NET_AP) {
    g_dns.processNextRequest();
    // Retry the saved networks in the background. Joining moves the radio to
    // the router's channel, which drops anyone on the setup AP, so wait while
    // a client is connected. A successful join reboots: the normal boot path
    // then brings everything up in station mode.
    if (!g_retryAllowed || !g_cfg || !g_cfg->wifiCount) return;
    bool busy = WiFi.softAPgetStationNum() > 0;
    if (g_attemptStart) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("[net] setup-mode retry joined, rebooting"));
        delay(100);
        ESP.restart();
      }
      uint32_t budget = g_seen[g_curNet] ? kJoinBudgetMs : kUnseenBudgetMs;
      if (busy || millis() - g_attemptStart > budget) {
        if (!busy) recordFailure(g_cfg->wifi[g_curNet].ssid);
        WiFi.disconnect();
        g_attemptStart = 0;
        g_retryAt = millis();
      }
    } else if (!busy && millis() - g_retryAt > kRetryEveryMs) {
      g_curNet = (int8_t)((g_curNet + 1) % g_cfg->wifiCount);
      beginJoin(g_cfg->wifi[g_curNet]);
      g_attemptStart = millis();
      if (!g_attemptStart) g_attemptStart = 1;   // 0 means idle
    }
    return;
  }
  // STA: keep mDNS alive, nudge reconnect if we dropped. After a long outage
  // rotate through the other saved networks. Never scan here — it would block
  // the display loop and the web server; WiFi.begin is fire-and-forget and its
  // status is picked up on later passes.
  platformMdnsUpdate();
  if (WiFi.status() == WL_CONNECTED) {
    g_downSince = 0;
    gatewayWatch();
    return;
  }
  if (!g_downSince) g_downSince = millis();
  if (millis() - g_downSince > kDownRestartMs && (!g_cfg || g_cfg->wifiCount <= 1)) watchdogRestart(WD_DOWN);
  if (millis() - g_lastReconnect > 10000) {
    g_lastReconnect = millis();
    if (g_cfg && g_cfg->wifiCount > 1 && millis() - g_downSince > 45000) {
      g_curNet = (int8_t)((g_curNet + 1) % g_cfg->wifiCount);
      beginJoin(g_cfg->wifi[g_curNet]);
      g_downSince = millis();   // give this candidate its own window before rotating on
    } else {
      WiFi.reconnect();         // first ~45 s: keep nudging the current network
    }
  }
}

NetMode netMode()      { return g_mode; }
bool    netConnected() { return g_mode == NET_STA && WiFi.status() == WL_CONNECTED; }

String netIP() {
  return (g_mode == NET_AP) ? WiFi.softAPIP().toString()
                            : WiFi.localIP().toString();
}

String netSSID() {
  return (g_mode == NET_AP) ? g_apSsid : WiFi.SSID();
}

int netRSSI() {
  return (g_mode == NET_STA) ? WiFi.RSSI() : 0;
}

void        netAllowRetry(bool on) { g_retryAllowed = on; }
const char* netFailReason() { return g_failText.c_str(); }
uint16_t    netFailSeq()    { return g_failSeq; }
