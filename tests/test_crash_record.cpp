// c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc tests/test_crash_record.cpp -o /tmp/geekmagic-crash-tests && /tmp/geekmagic-crash-tests
#include "CrashRecord.h"
#include <cassert>
#include <cstring>
#include <iostream>

static CrashRtc sample() {
  CrashRtc r;
  memset(&r, 0, sizeof(r));
  r.magic = CRASH_MAGIC;
  r.exccause = 28; r.epc1 = 0x40201234; r.excvaddr = 0; r.uptimeS = 77;
  r.rstReason = CRASH_RST_EXCEPTION; r.uptimeKnown = 1;
  strcpy(r.fw, "1.35");
  r.check = crashCheck(r);
  return r;
}

static void testTrace() {
  // Code addresses only, newest first, at most four, without the fault itself.
  const uint32_t stack[] = {0x3ffffa00, 0x4027d7b6, 0x40212340, 7, 0x40100abc, 0x60000000, 0x40234560, 0x40245670, 0x40256780};
  uint32_t t[4] = {0, 0, 0, 0};
  crashTrace(stack, stack + 9, 0x4027d7b6, t);
  assert(t[0] == 0x40212340 && t[1] == 0x40100abc && t[2] == 0x40234560 && t[3] == 0x40245670);
  uint32_t none[4] = {0, 0, 0, 0};
  crashTrace(stack, stack, 0, none);
  assert(!none[0]);
  assert(!strcmp(loopPhaseLabel(PHASE_WEATHER), "weather fetch") && !strcmp(loopPhaseLabel(0), ""));
}

int main() {
  testTrace();
  // The record is 18 words and must end inside the 128 user words from word 96.
  assert(sizeof(CrashRtc) == 72);
  assert(96 + sizeof(CrashRtc) / 4 <= 128);

  // Labels: 253/254 get their own, the hardware watchdog is only REASON_WDT_RST.
  assert(!strcmp(crashReasonLabel(2), "Exception"));
  assert(!strcmp(crashReasonLabel(3), "Software watchdog"));
  assert(!strcmp(crashReasonLabel(1), "Hardware watchdog"));
  assert(!strcmp(crashReasonLabel(254), "Abort/out of memory"));
  assert(!strcmp(crashReasonLabel(253), "Stack overflow"));
  assert(!strcmp(crashReasonLabel(0), "Unknown"));
  assert(!strcmp(crashReasonLabel(4), "Unknown"));
  assert(!strcmp(crashReasonLabel(255), "Unknown"));
  assert(strlen(crashReasonLabel(254)) < 24);   // PlatformCrashRecord::reason[24]

  // The checksum covers every byte after the two header words, and not the checksum itself.
  CrashRtc a = sample();
  assert(a.check == crashCheck(a));
  CrashRtc b = a; b.epc1++;
  assert(b.check != crashCheck(b));
  b = a; b.fw[0] = 'x';
  assert(b.check != crashCheck(b));
  b = a; b.flags = CRASH_PENDING;
  assert(b.check != crashCheck(b));

  // A firmware field with no terminator is cut, and the copy always ends in one.
  {
    CrashRtc u; memset(&u, 'x', sizeof(u));
    char out[16]; memset(out, 'y', sizeof(out));
    crashCopyFw(out, sizeof(out), u);
    assert(strlen(out) == 15 && out[15] == 0);
    char small[4];
    crashCopyFw(small, sizeof(small), sample());
    assert(!strcmp(small, "1.3"));
    crashCopyFw(out, sizeof(out), sample());
    assert(!strcmp(out, "1.35"));
  }

  // Abort and stack overflow carry no registers; the other reasons do.
  assert(!crashHasRegisters(254) && !crashHasRegisters(253));
  assert(crashHasRegisters(1) && crashHasRegisters(2) && crashHasRegisters(3) && crashHasRegisters(0));

  // Boot decisions.
  assert(crashBootAction(true, true, 4)  == CRASH_ADOPT);    // abort(): comes back as a software restart
  assert(crashBootAction(true, true, 2)  == CRASH_ADOPT);    // exception
  assert(crashBootAction(true, true, 3)  == CRASH_ADOPT);    // software watchdog
  assert(crashBootAction(true, false, 1) == CRASH_CAPTURE);  // hardware watchdog: no callback ran
  assert(crashBootAction(false, false, 1) == CRASH_CAPTURE);
  assert(crashBootAction(true, false, 2) == CRASH_CAPTURE);  // exception whose callback write was lost
  assert(crashBootAction(true, false, 4) == CRASH_KEEP);     // normal software restart keeps the record
  assert(crashBootAction(true, false, 6) == CRASH_KEEP);     // external reset
  assert(crashBootAction(false, false, 0) == CRASH_KEEP);    // power on, nothing stored
  assert(crashBootAction(true, false, 0) == CRASH_KEEP);

  std::cout << "crash record tests passed\n";
  return 0;
}
