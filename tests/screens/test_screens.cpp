// The firmware's own screens (ScreenView), on the desktop: every screen stays
// inside the safe area, values step down a style instead of overflowing, the
// splash bar fills as asked, the setup QR code lands on its card intact, and
// with an output path argument the screens are written side by side as a PPM:
//   tests/screens/run.sh /tmp/screens.ppm
#include "ScreenView.h"
#include "SysText.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace screenview;

struct Frame { std::vector<uint16_t> px = std::vector<uint16_t>(kWidth * kHeight, 0); int rows = 0; };
static void sink(void* context, int y, const uint16_t* row) {
  Frame* f = static_cast<Frame*>(context);
  assert(y >= 0 && y < kHeight);
  std::memcpy(&f->px[y * kWidth], row, kWidth * sizeof(uint16_t));
  ++f->rows;
}
static void inSafeArea(const Frame& f) {
  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x)
      if (f.px[y * kWidth + x]) assert(x >= kSafe - 2 && x < kWidth - kSafe + 2 && y >= kSafe - 2 && y < kHeight - kSafe + 2);
}
static Frame splash(const char* detail, int pct, Splash& sp) {
  Frame f;
  splashLayout(detail, pct, sp);
  for (int y = 0; y < kHeight; ++y) splashRow(sp, y, &f.px[y * kWidth]);
  inSafeArea(f);
  return f;
}
static Frame qrPage(const char* ssid, const char* pass, QrPage& p, qr::Code& code) {
  char payload[160];
  assert(qr::wifiPayload(ssid, pass, payload, sizeof(payload)));
  assert(qr::encode(reinterpret_cast<const uint8_t*>(payload), std::strlen(payload), code));
  qrLayout(code, p);
  Frame f;
  const Span span = renderQr(p, "Wi-Fi setup", "Scan to join", kWarn, sink, &f);
  assert(span.top >= kSafe && span.bottom <= kHeight - kSafe);
  inSafeArea(f);
  // Every module's centre pixel is black when dark and white when light.
  for (int my = 0; my < code.size; ++my)
    for (int mx = 0; mx < code.size; ++mx) {
      const int x = p.cardLeft + (mx + 2) * p.module + p.module / 2, y = p.cardTop + (my + 2) * p.module + p.module / 2;
      assert(f.px[y * kWidth + x] == (code.dark(mx, my) ? 0 : 0xFFFF));
    }
  return f;
}

static Frame draw(const Line* lines, int count, uint16_t accent) {
  Frame f;
  const Span span = render(lines, count, accent, sink, &f);
  assert(span.top >= kSafe && span.bottom <= kHeight - kSafe);   // the case covers the edges
  assert(f.rows == span.bottom - span.top);
  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x)
      if (f.px[y * kWidth + x]) assert(x >= kSafe - 2 && x < kWidth - kSafe + 2);
  return f;
}

int main(int argc, char** argv) {
  Line l[kMaxLines];
  Scratch s;
  std::vector<Frame> frames;
  Splash sp;
  frames.push_back(splash("1.36", 5, sp));
  frames.push_back(splash("Joining Очень длинное имя домашней сети", 45, sp));
  {   // the bar: track at 0 %, white up to the share asked for
    const int y = sp.barTop + kBarH / 2;
    Frame f = splash("x", 50, sp);
    assert(f.px[y * kWidth + sp.barLeft + 20] == kValue && f.px[y * kWidth + sp.barLeft + 70] == kTrack);
    f = splash("x", -1, sp);
    for (int x = 0; x < kWidth; ++x) assert(f.px[y * kWidth + x] == 0);
  }
  frames.push_back(splash("Restarting", 100, sp));
  QrPage qp;
  qr::Code code;
  frames.push_back(qrPage("gm-hotspot", "secret-pass", qp, code));
  assert(code.size == 29 && qp.module == 4);
  frames.push_back(qrPage("Очень длинное имя сети", std::string(63, 'p').c_str(), qp, code));
  assert(code.size > 29 && qp.module >= 2);
  frames.push_back(draw(l, setupLines("GeekMagic-Setup", "", "192.168.4.1", nullptr, s, l), kWarn));
  frames.push_back(draw(l, setupLines("gm-hotspot", "secret-pass", "192.168.4.1", "Wrong password for Wi-Fi", s, l), kWarn));
  frames.push_back(draw(l, connectedLines("Wi-Fi", "192.168.1.42", "geekmagic", s, l), kOk));
  frames.push_back(draw(l, connectedLines("Очень длинное имя домашней сети", "192.168.1.42", "geekmagic", s, l), kOk));
  frames.push_back(draw(l, crashLines("0x40212345", "0x00000000", "192.168.1.42", s, l), kBad));
  // The same screens in Russian (SysText): every word in both languages, and
  // the longest Russian lines still fit the safe area.
  {
    char w[64], ru[64];
    for (int i = 0; i < sys::WordCount; ++i) {
      sys::textIn(0, (sys::Word)i, w, sizeof(w));
      sys::textIn(1, (sys::Word)i, ru, sizeof(ru));
      assert(w[0] && ru[0] && (unsigned char)ru[0] >= 0x20);
    }
    assert(std::string(sys::textIn(1, sys::Joining, ru, sizeof(ru))) == "Подключение к %s");
    assert(std::string(sys::textIn(0, sys::WifiSetup, w, sizeof(w))) == "Wi-Fi setup");
    assert(std::string(sys::textIn(1, sys::NotifyWaiting, ru, sizeof(ru))) == "ЖДЁТ ВАС");
    sys::setLanguage(1);
    assert(sys::language() == 1);
    frames.push_back(draw(l, setupLines("gm-hotspot", "secret-pass", "192.168.4.1", "Home: неверный пароль? (15)", s, l), kWarn));
    frames.push_back(draw(l, connectedLines("Wi-Fi", "192.168.1.42", "geekmagic", s, l), kOk));
    frames.push_back(draw(l, crashLines("0x40212345", "0x00000000", "192.168.1.42", s, l), kBad));
    Splash sp;
    frames.push_back(splash(sys::text(sys::Joining, w, sizeof(w)), 40, sp));
    frames.push_back(splash(sys::text(sys::UpdateFailed, w, sizeof(w)), -1, sp));
    sys::setLanguage(7);
    assert(sys::language() == 0);   // anything else: English
  }
  // Line counts: optional lines appear only when there is something to say.
  assert(setupLines("x", "", "1", nullptr, s, l) == 4 && setupLines("x", "p", "1", "why", s, l) == 6);
  assert(connectedLines("x", "1", "", s, l) == 4 && connectedLines("x", "1", "h", s, l) == 5);

  if (argc > 1) {
    const int n = static_cast<int>(frames.size()), gap = 12, W = n * kWidth + (n - 1) * gap;
    std::vector<unsigned char> out(W * kHeight * 3, 60);
    for (int i = 0; i < n; ++i)
      for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x) {
          const uint16_t c = frames[i].px[y * kWidth + x];
          unsigned char* o = &out[(y * W + i * (kWidth + gap) + x) * 3];
          o[0] = ((c >> 11) * 255) / 31;
          o[1] = (((c >> 5) & 63) * 255) / 63;
          o[2] = ((c & 31) * 255) / 31;
        }
    FILE* f = std::fopen(argv[1], "wb");
    assert(f);
    std::fprintf(f, "P6\n%d %d\n255\n", W, kHeight);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
  }
  std::cout << "screen tests passed\n";
}
