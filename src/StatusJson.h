#pragma once
// GET /api/status, written straight to the client in small pieces.
//
// Earlier the handler built an ArduinoJson document (about 1 KB of small
// blocks), serialized it into one String (another 0.5 KB) and handed that to
// the web server, which copies it once more for lwIP. Six requests arriving
// together, with the 5 connections the server holds waiting, left a the original
// device with a 648-byte largest block, and a diag build crashed (the Wi-Fi
// driver could not get 384 bytes). Here every value is written the moment it
// is known, through a PieceWriter, and only the piece buffer (128 bytes) and
// one tiny document for the single value being written are alive.
//
// The bytes are exactly the ones the original reply had: same keys, same order, every
// value serialized by ArduinoJson (escapes, number format). The caller
// supplies the values; this file knows nothing of the firmware, so a native
// test can compare it with the old way, byte for byte.
//
// Call order: writeStatusHead(), then writeStatusCrash() if there is a crash
// record, and writeStatusEnd().
#include "JsonStream.h"

namespace gm {
// The values behind the first part of the reply. Strings are borrowed: they
// must live until writeStatusHead() returns.
struct StatusData {
  const char *fw = "", *version = "", *variant = "";
  const char *mode = "", *ssid = "", *ip = "";
  const char* wifiFail = "";    // left out when empty
  const char* reset = "";
  const char* time = "";        // left out when empty
  const char* tz = "";
  const char* sun = "";         // today's sunrise-sunset; left out when empty
  bool connected = false, synced = false, night = false, nightHeld = false, clockFresh = false;
  int rssi = 0;
  int bl = 0;                   // backlight level now, 0..255
  uint32_t heap = 0, maxblk = 0, contstk = 0, uptime = 0;
  uint32_t minHeap = 0;         // lowest free heap seen since boot; left out when 0
  // The station link's health (Net.h netHealth); left out unless hasNet.
  bool hasNet = false;
  uint32_t drops = 0, rejoins = 0;
  int gateway = -1, silentFor = 0;
  const char* lastDrop = "";
  const char* lastRestart = "";
};
// The record of the last crash (Platform.h). Only written when there is one.
// The register strings are left out when empty.
struct StatusCrash {
  const char *reason = "", *fw = "";
  const char *exccause = "", *epc = "", *addr = "";
  bool hasUptime = false;
  uint32_t uptime = 0;
};
namespace statusout {
// `,"key":` (the key is a PSTR literal with its comma and quotes), then the value.
template <class Out> void member(Out& out, const char* key, const char* v) {
  jsonout::raw(out, key);
  jsonout::text(out, v);
}
template <class Out, class T> void member(Out& out, const char* key, T v) {
  jsonout::raw(out, key);
  jsonout::value(out, v);
}
}

// Everything but the crash record. No ArduinoJson document holds more
// than the one value being written: a document with an object in it takes a
// 1 KB block of heap in one piece, which the ESP8266 may not have.
template <class Out> void writeStatusHead(Out& out, const StatusData& d) {
  using statusout::member;
  member(out, PSTR("{\"fw\":"), d.fw);
  member(out, PSTR(",\"version\":"), d.version);
  member(out, PSTR(",\"variant\":"), d.variant);
  member(out, PSTR(",\"mode\":"), d.mode);
  member(out, PSTR(",\"connected\":"), d.connected);
  member(out, PSTR(",\"ssid\":"), d.ssid);
  member(out, PSTR(",\"ip\":"), d.ip);
  member(out, PSTR(",\"rssi\":"), d.rssi);
  if (d.wifiFail[0]) member(out, PSTR(",\"wifiFail\":"), d.wifiFail);
  member(out, PSTR(",\"heap\":"), d.heap);
  member(out, PSTR(",\"maxblk\":"), d.maxblk);
  member(out, PSTR(",\"contstk\":"), d.contstk);
  member(out, PSTR(",\"uptime\":"), d.uptime);
  member(out, PSTR(",\"reset\":"), d.reset);
  member(out, PSTR(",\"synced\":"), d.synced);
  if (d.time[0]) member(out, PSTR(",\"time\":"), d.time);
  member(out, PSTR(",\"tz\":"), d.tz);
  member(out, PSTR(",\"night\":"), d.night);
  member(out, PSTR(",\"nightHeld\":"), d.nightHeld);
  member(out, PSTR(",\"clockFresh\":"), d.clockFresh);
  if (d.sun[0]) member(out, PSTR(",\"sun\":"), d.sun);
  member(out, PSTR(",\"bl\":"), d.bl);
  if (d.minHeap) member(out, PSTR(",\"minHeap\":"), d.minHeap);
  if (d.hasNet) {
    member(out, PSTR(",\"net\":{\"drops\":"), d.drops);
    if (d.lastDrop[0]) member(out, PSTR(",\"lastDrop\":"), d.lastDrop);
    member(out, PSTR(",\"gateway\":"), d.gateway == 1 ? "answering" : d.gateway == 0 ? "silent" : "unknown");
    member(out, PSTR(",\"silentFor\":"), d.silentFor);
    member(out, PSTR(",\"rejoins\":"), d.rejoins);
    if (d.lastRestart[0]) member(out, PSTR(",\"lastRestart\":"), d.lastRestart);
    jsonout::raw(out, PSTR("}"));
  }
}

// `,"lastCrash":{...}`, added after the original keys: nothing that read the reply before reads it.
template <class Out> void writeStatusCrash(Out& out, const StatusCrash& k) {
  using statusout::member;
  member(out, PSTR(",\"lastCrash\":{\"reason\":"), k.reason);
  member(out, PSTR(",\"fw\":"), k.fw);
  if (k.exccause[0]) member(out, PSTR(",\"exccause\":"), k.exccause);
  if (k.epc[0]) member(out, PSTR(",\"epc\":"), k.epc);
  if (k.addr[0]) member(out, PSTR(",\"addr\":"), k.addr);
  if (k.hasUptime) member(out, PSTR(",\"uptime\":"), k.uptime);
  jsonout::raw(out, PSTR("}"));
}

// Closes the object and sends the last piece.
template <class Out> void writeStatusEnd(Out& out) {
  jsonout::raw(out, PSTR("}"));
  out.flush();
}
}
