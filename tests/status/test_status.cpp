// GET /api/status is written straight to the client in pieces (StatusJson.h).
// Its bytes must stay exactly what the original reply had: the web UI, Home Assistant,
// n8n and the device tests all read it. The reference below is the way
// handleStatus() built the reply earlier (one ArduinoJson document,
// serialized whole), fed the same values.
#include "StatusJson.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace gm;

static StatusData idleData();
struct Case {
  explicit Case(const char* n) : name(n), d(idleData()) {}
  const char* name;
  StatusData d;
};

// the old handleStatus(), field by field.
static std::string reference(const Case& c) {
  const StatusData& d = c.d;
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  o["fw"] = d.fw;
  o["version"] = d.version;
  o["variant"] = d.variant;
  o["mode"] = d.mode;
  o["connected"] = d.connected;
  o["ssid"] = std::string(d.ssid);
  o["ip"] = std::string(d.ip);
  o["rssi"] = d.rssi;
  if (d.wifiFail[0]) o["wifiFail"] = d.wifiFail;
  o["heap"] = d.heap;
  o["maxblk"] = d.maxblk;
  o["contstk"] = d.contstk;
  o["uptime"] = d.uptime;
  o["reset"] = d.reset;
  o["synced"] = d.synced;
  if (d.time[0]) o["time"] = std::string(d.time);
  o["tz"] = std::string(d.tz);
  o["night"] = d.night;
  o["nightHeld"] = d.nightHeld;
  o["clockFresh"] = d.clockFresh;
  if (d.sun[0]) o["sun"] = std::string(d.sun);
  o["bl"] = d.bl;
  if (d.minHeap) o["minHeap"] = d.minHeap;
  if (d.hasNet) {
    JsonObject n = o["net"].to<JsonObject>();
    n["drops"] = d.drops;
    if (d.lastDrop[0]) n["lastDrop"] = d.lastDrop;
    n["gateway"] = d.gateway == 1 ? "answering" : d.gateway == 0 ? "silent" : "unknown";
    n["silentFor"] = d.silentFor;
    n["rejoins"] = d.rejoins;
    if (d.lastRestart[0]) n["lastRestart"] = d.lastRestart;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

struct StringSink : Sink {
  std::string text;
  size_t pieces = 0, longest = 0;
  bool write(const char* data, size_t count) override {
    text.append(data, count);
    pieces++;
    if (count > longest) longest = count;
    return true;
  }
};

template <size_t Piece> static std::string streamed(const Case& c, StringSink& sink) {
  PieceWriter<Piece> out(sink);
  writeStatusHead(out, c.d);
  writeStatusEnd(out);
  return sink.text;
}

static StatusData idleData() {
  StatusData d;
  d.fw = "geekmagic";
  d.version = "1.35";
  d.variant = "esp8266";
  d.mode = "sta";
  d.connected = true;
  d.ssid = "home-wifi-01";
  d.ip = "198.51.100.177";
  d.rssi = -67;
  d.heap = 6384;
  d.maxblk = 3080;
  d.contstk = 660;
  d.uptime = 290;
  d.reset = "Software/System restart";
  d.synced = true;
  d.time = "2026-11-01 11:00";
  d.tz = "";
  d.clockFresh = true;
  return d;
}

static std::vector<Case> cases() {
  std::vector<Case> all;
  // Saved from the device (Pixel Room, nothing configured).
  Case idle("idle");
  all.push_back(idle);

  // Every optional field present, text that needs escaping, large and
  // negative numbers.
  Case full("full");
  full.d.wifiFail = "wrong password\n(try again)";
  full.d.ssid = "caf\xc3\xa9 \"WiFi\" \\ 5G";
  full.d.rssi = -91;
  full.d.heap = 4294967295u;
  full.d.maxblk = 0;
  full.d.uptime = 4000000000u;
  full.d.tz = "Europe/Rome";
  full.d.mode = "ap";
  full.d.minHeap = 15872;
  full.d.hasNet = true;
  full.d.drops = 3;
  full.d.lastDrop = "signal lost";
  full.d.gateway = 0;
  full.d.silentFor = 4;
  full.d.rejoins = 1;
  full.d.lastRestart = "router silent for 5 minutes";
  full.d.connected = false;
  full.d.synced = false;
  full.d.time = "2026-11-01 11:00";   // every optional field is present in this one
  full.d.night = true;
  full.d.nightHeld = true;
  full.d.clockFresh = false;
  full.d.sun = "06:58-16:42";
  full.d.bl = 255;
  all.push_back(full);

  // The clock not yet synced: no "time" key.
  Case early("unsynced");
  early.d.time = "";
  all.push_back(early);

  // The sun text with a dimmed backlight (a middle level), and a sun text
  // that needs escaping; and no sun text with the backlight fully on.
  Case dim("sun-dim");
  dim.d.sun = "polar \"day\"";
  dim.d.bl = 37;
  all.push_back(dim);
  Case lit("no-sun-lit");
  lit.d.sun = "";
  lit.d.bl = 255;
  all.push_back(lit);
  return all;
}

int main() {
  // The device's reply as saved on 1 November 2026, without the fields of the
  // removed GitHub updater and WireGuard tunnel (repo, wg): every other byte.
  const char* saved =
      "{\"fw\":\"geekmagic\",\"version\":\"1.35\",\"variant\":\"esp8266\","
      "\"mode\":\"sta\",\"connected\":true,\"ssid\":\"home-wifi-01\",\"ip\":\"198.51.100.177\",\"rssi\":-67,\"heap\":6384,"
      "\"maxblk\":3080,\"contstk\":660,\"uptime\":290,\"reset\":\"Software/System restart\",\"synced\":true,"
      "\"time\":\"2026-11-01 11:00\",\"tz\":\"\",\"night\":false,\"nightHeld\":false,\"clockFresh\":true,\"bl\":0}";
  int checked = 0;
  for (const Case& c : cases()) {
    const std::string want = reference(c);
    if (std::string(c.name) == "idle") {
      if (want != saved) { std::fprintf(stderr, "reference no longer matches the saved reply:\n%s\n%s\n", want.c_str(), saved); return 1; }
    }
    // Where the sun fields sit is fixed: after clockFresh, then minHeap and net.
    if (std::string(c.name) == "full" &&
        want.find("\"clockFresh\":false,\"sun\":\"06:58-16:42\",\"bl\":255,\"minHeap\":15872,\"net\":{") == std::string::npos) {
      std::fprintf(stderr, "sun and bl are not right after clockFresh:\n%s\n", want.c_str());
      return 1;
    }
    if (std::string(c.name) == "no-sun-lit" && (want.find("\"sun\"") != std::string::npos ||
                                                want.find("\"clockFresh\":true,\"bl\":255}") == std::string::npos)) {
      std::fprintf(stderr, "an empty sun text must be left out; bl stays:\n%s\n", want.c_str());
      return 1;
    }
    StringSink big, small;
    const std::string a = streamed<128>(c, big);
    const std::string b = streamed<5>(c, small);   // pieces that split every token
    if (a != want || b != want) {
      std::fprintf(stderr, "%s: bytes differ\n want %s\n got  %s\n 5 B  %s\n", c.name, want.c_str(), a.c_str(), b.c_str());
      return 1;
    }
    // Pieces: all but the last are full, none empty.
    assert(big.longest <= 128 && small.longest <= 5);
    assert(big.pieces == (want.size() + 127) / 128);
    assert(small.pieces == (want.size() + 4) / 5);
    checked++;
  }
  {  // lastCrash: after the original keys, registers left out when empty
    StatusCrash k;
    k.reason = "Exception"; k.fw = "1.35"; k.exccause = "0x1d"; k.epc = "0x40201234"; k.addr = "0x00000000";
    k.hasUptime = true; k.uptime = 77;
    StringSink s1;
    { PieceWriter<5> out(s1); writeStatusCrash(out, k); out.flush(); }
    assert(s1.text == ",\"lastCrash\":{\"reason\":\"Exception\",\"fw\":\"1.35\",\"exccause\":\"0x1d\",\"epc\":\"0x40201234\",\"addr\":\"0x00000000\",\"uptime\":77}");
    StatusCrash a;
    a.reason = "Abort/out of memory"; a.fw = "1.35";
    StringSink s2;
    { PieceWriter<128> out(s2); writeStatusCrash(out, a); out.flush(); }
    assert(s2.text == ",\"lastCrash\":{\"reason\":\"Abort/out of memory\",\"fw\":\"1.35\"}");
    StatusCrash w;   // what the firmware was doing, the heap and the stack's code addresses
    w.reason = "Software watchdog"; w.fw = "1.47"; w.phase = "weather fetch"; w.sleep = "none";
    w.heap = 21000; w.minHeap = 9000; w.trace = "0x40212340 0x40234560";
    StringSink s3;
    { PieceWriter<7> out(s3); writeStatusCrash(out, w); out.flush(); }
    assert(s3.text == ",\"lastCrash\":{\"reason\":\"Software watchdog\",\"fw\":\"1.47\",\"phase\":\"weather fetch\",\"sleep\":\"none\","
                      "\"heap\":21000,\"minHeap\":9000,\"trace\":\"0x40212340 0x40234560\"}");
  }
  std::printf("status reply tests passed (%d replies, byte for byte)\n", checked);
  return 0;
}
