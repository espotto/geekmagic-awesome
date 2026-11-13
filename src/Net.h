// Net.h — WiFi station / fallback AP / captive portal / mDNS
#pragma once
#include <Arduino.h>
#include "Settings.h"

enum NetMode { NET_STA, NET_AP };

// Connects to the configured station; falls back to AP if that fails or no
// credentials are stored. In AP mode with saved networks, netLoop retries them
// in the background and reboots once one joins. `onProgress` (optional) is called with short status
// strings and a rough share of the boot done (0-100) for the start-up screen.
void netBegin(const Settings& s, void (*onProgress)(const char*, uint8_t) = nullptr);
void netLoop();           // pump DNS (AP) / mDNS (STA) / reconnect

NetMode  netMode();
bool     netConnected();  // STA associated with an IP
String   netIP();         // current IP (STA or AP)
String   netSSID();       // joined SSID (STA) or AP SSID
int      netRSSI();       // STA signal, 0 in AP mode

// Setup-mode retry on/off (on by default). Off in safe mode: a join there
// reboots, and a software restart would clear the crash flag that holds the
// device in safe mode.
void        netAllowRetry(bool on);

// Last failed join, e.g. "Home: wrong password? (15)"; empty once joined.
const char* netFailReason();
uint16_t    netFailSeq();     // changes whenever netFailReason() does

// The station link's health, for /api/status. The network watchdog (ESP8266)
// pings the router every 30 s once it has answered a ping: after 2 minutes of
// silence it rejoins, after 5 it restarts the device; a link that stays down
// 10 minutes restarts it too. A restart of its own is remembered in RTC
// memory (it survives a restart, not a power cut) and reported once.
struct NetHealth {
  uint32_t drops = 0;          // disconnects since boot
  char lastDrop[24] = "";      // why the last one happened (driver reason)
  int8_t gateway = -1;         // router answering pings: 1 yes, 0 not now, -1 never answered (watchdog off)
  uint8_t silentFor = 0;       // pings in a row without an answer
  uint32_t rejoins = 0;        // watchdog rejoins since boot
  char lastRestart[40] = "";   // a restart the watchdog made before this boot
};
void netHealth(NetHealth& out);
