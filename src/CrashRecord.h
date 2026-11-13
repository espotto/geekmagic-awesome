#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Layout and decisions of the ESP8266 crash record, kept free of Arduino and
// SDK headers so tests/test_crash_record.cpp can build them on the host.

// rst_info.reason values, as in the SDK's user_interface.h plus the two the
// core's postmortem handler passes to custom_crash_callback() on its own.
enum CrashReasonCode : uint8_t {
  CRASH_RST_WDT         = 1,    // hardware watchdog
  CRASH_RST_EXCEPTION   = 2,
  CRASH_RST_SOFT_WDT    = 3,
  CRASH_RST_STACK_SMASH = 253,  // core: stack smashing detected
  CRASH_RST_SW_EXC      = 254,  // core: abort(), panic(), out of memory
};

// What the main loop was doing, kept in a global the crash callback reads: a
// fault inside the SDK (the Wi-Fi driver's own watchdog loops) shows only an
// SDK address, and this says what the firmware had asked of it.
enum LoopPhase : uint8_t {
  PHASE_UNKNOWN = 0,
  PHASE_NET,       // netLoop(): joining, the gateway ping, mDNS
  PHASE_WEB,       // a web request
  PHASE_APPS,      // drawing an app, notices, the night clock, or waiting
  PHASE_WEATHER,   // the Weather app's HTTP request
  PHASE_FACEDATA,  // a watch face's own HTTP data request
  PHASE_SETUP,     // setup(), before the first loop
};
static inline const char* loopPhaseLabel(uint8_t p) {
  switch (p) {
    case PHASE_NET:      return "network";
    case PHASE_WEB:      return "web request";
    case PHASE_APPS:     return "apps";
    case PHASE_WEATHER:  return "weather fetch";
    case PHASE_FACEDATA: return "watch face data";
    case PHASE_SETUP:    return "start";
    default:             return "";
  }
}

static const uint32_t CRASH_MAGIC   = 0x43525348;   // "CRSH"
static const uint32_t CRASH_PENDING = 1;            // written by the callback, not yet adopted

struct CrashRtc {
  uint32_t magic;
  uint32_t check;       // FNV-1a over every byte after this field
  uint32_t flags;       // CRASH_PENDING
  uint32_t exccause, epc1, excvaddr;
  uint32_t uptimeS;
  uint8_t  rstReason;   // CrashReasonCode
  uint8_t  uptimeKnown;
  uint8_t  phase;       // LoopPhase then; 0 unknown
  uint8_t  wifiSleep;   // the Wi-Fi sleep type then: 0 none, 1 light, 2 modem; 255 unknown
  char     fw[16];
  uint32_t heap;        // free heap then; 0 unknown
  uint32_t minHeap;     // the lowest free heap since boot; 0 unknown
  uint32_t trace[4];    // the first code addresses on the stack, for addr2line; 0 none
};

// Flash code (irom) and IRAM code: what a return address on the stack can be.
static inline bool crashIsCode(uint32_t a) {
  return (a >= 0x40200000u && a < 0x40300000u) || (a >= 0x40100000u && a < 0x40108000u);
}
// Fills `trace` with the first code addresses in the words [p, end), leaving
// out `epc` (the fault itself): the callers that led to the fault, newest first.
static inline void crashTrace(const uint32_t* p, const uint32_t* end, uint32_t epc, uint32_t trace[4]) {
  int n = 0;
  for (int words = 0; p < end && n < 4 && words < 512; ++p, ++words)
    if (crashIsCode(*p) && *p != epc) trace[n++] = *p;
}

static inline uint32_t crashCheck(const CrashRtc& r) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&r) + 8;
  uint32_t h = 2166136261u;
  for (size_t i = 8; i < sizeof(r); i++) { h ^= *p++; h *= 16777619u; }
  return h;
}

// Copy a firmware string out of RTC memory. The record passed its checksum but
// the bytes are still untrusted: the field is cut at its end whether or not a
// terminator is there, and the copy always ends in one.
static inline void crashCopyFw(char* dst, size_t dstSize, const CrashRtc& r) {
  size_t n = 0;
  while (n < sizeof r.fw && r.fw[n]) n++;
  if (n > dstSize - 1) n = dstSize - 1;
  memcpy(dst, r.fw, n);
  dst[n] = 0;
}

static inline const char* crashReasonLabel(uint8_t reason) {
  switch (reason) {
    case CRASH_RST_EXCEPTION:   return "Exception";
    case CRASH_RST_SOFT_WDT:    return "Software watchdog";
    case CRASH_RST_WDT:         return "Hardware watchdog";
    case CRASH_RST_SW_EXC:      return "Abort/out of memory";
    case CRASH_RST_STACK_SMASH: return "Stack overflow";
    default:                    return "Unknown";
  }
}

// The core zeroes rst_info before it calls custom_crash_callback() for an
// abort and for a stack smash, so there is no exccause, epc1 or excvaddr to
// report for them: showing 0x0 would read as a real fault at address 0.
static inline bool crashHasRegisters(uint8_t reason) {
  return reason != CRASH_RST_SW_EXC && reason != CRASH_RST_STACK_SMASH;
}

// What platformCrashRecordBegin() does with the record at boot.
enum CrashBootAction { CRASH_KEEP, CRASH_ADOPT, CRASH_CAPTURE };
// A record the callback wrote is adopted whatever the boot reason says: after
// an abort() or a stack smash the chip restarts through a plain software
// restart, so the reset reason alone can't tell it from a normal one. Without
// one, only the three reasons the ROM reports for a crash build a record from
// rst_info (the hardware watchdog never reaches the callback). Anything else is
// a normal restart and leaves the stored record alone.
static inline CrashBootAction crashBootAction(bool haveRecord, bool pending, uint32_t bootReason) {
  if (haveRecord && pending) return CRASH_ADOPT;
  if (bootReason == CRASH_RST_WDT || bootReason == CRASH_RST_EXCEPTION ||
      bootReason == CRASH_RST_SOFT_WDT) return CRASH_CAPTURE;
  return CRASH_KEEP;
}
