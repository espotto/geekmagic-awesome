// QrCode.h — a small QR Code encoder for the Wi-Fi setup screen: byte mode,
// versions 1-8, error correction M when it fits and L otherwise, the mask
// chosen by the standard penalty rules. Free of Arduino, so the desktop tests
// it against a reference encoder (tests/screens).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace qr {

constexpr int kMaxVersion = 8;
constexpr int kMaxSize = 4 * kMaxVersion + 17;               // 49 modules
constexpr int kBitmapBytes = (kMaxSize * kMaxSize + 7) / 8;

struct Code {
  int size = 0;                     // modules per side, 0 when encoding failed
  uint8_t bits[kBitmapBytes];       // row-major, 1 = dark
  bool dark(int x, int y) const {
    const int i = y * size + x;
    return (bits[i >> 3] >> (i & 7)) & 1;
  }
};

// Encodes `len` bytes. `forceVersion`/`forceMask`/`forceEcc` (0 = M, 1 = L)
// are for the tests; -1 picks them as described above. False if the data does
// not fit version 8 at level L.
bool encode(const uint8_t* data, size_t len, Code& out,
            int forceVersion = -1, int forceMask = -1, int forceEcc = -1);

// The Wi-Fi join payload phones understand ("WIFI:T:WPA;S:...;P:...;;"),
// escaping \ ; , : and ". An empty password makes it an open network. Returns
// false if it does not fit `cap`.
bool wifiPayload(const char* ssid, const char* pass, char* out, size_t cap);

}  // namespace qr
