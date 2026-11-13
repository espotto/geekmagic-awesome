#include "QrCode.h"
#include <string.h>

// The structure follows the QR Code specification (ISO/IEC 18004) in the order
// an encoder needs it: data bits, Reed-Solomon blocks, function patterns, the
// zigzag placement, then the mask.
namespace qr {
namespace {

// Error-correction codewords per block and number of blocks, versions 1-8,
// for level M (index 0) and L (index 1).
const uint8_t kEccPerBlock[2][kMaxVersion + 1] = {
  {0, 10, 16, 26, 18, 24, 16, 18, 22},
  {0, 7, 10, 15, 20, 26, 18, 20, 24},
};
const uint8_t kBlocks[2][kMaxVersion + 1] = {
  {0, 1, 1, 1, 2, 2, 4, 4, 4},
  {0, 1, 1, 1, 1, 1, 2, 2, 2},
};
// The format bits' two-bit level code: L is 01, M is 00.
const uint8_t kEccFormat[2] = {0, 1};

int rawDataModules(int ver) {
  int r = (16 * ver + 128) * ver + 64;
  if (ver >= 2) {
    const int align = ver / 7 + 2;
    r -= (25 * align - 10) * align - 55;
    if (ver >= 7) r -= 36;
  }
  return r;
}
int dataCodewords(int ver, int ecc) {
  return rawDataModules(ver) / 8 - kEccPerBlock[ecc][ver] * kBlocks[ecc][ver];
}

struct Grid {
  int size;
  uint8_t* bits;
  bool get(int x, int y) const { const int i = y * size + x; return (bits[i >> 3] >> (i & 7)) & 1; }
  void set(int x, int y, bool v) {
    const int i = y * size + x;
    if (v) bits[i >> 3] |= static_cast<uint8_t>(1 << (i & 7));
    else bits[i >> 3] &= static_cast<uint8_t>(~(1 << (i & 7)));
  }
};

// ---- Reed-Solomon over GF(256), polynomial 0x11D -------------------------
uint8_t gfMul(uint8_t a, uint8_t b) {
  int z = 0;
  for (int i = 7; i >= 0; --i) {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((b >> i) & 1) * a;
  }
  return static_cast<uint8_t>(z);
}
void rsDivisor(int degree, uint8_t* out) {
  memset(out, 0, degree);
  out[degree - 1] = 1;
  uint8_t root = 1;
  for (int i = 0; i < degree; ++i) {
    for (int j = 0; j < degree; ++j) {
      out[j] = gfMul(out[j], root);
      if (j + 1 < degree) out[j] ^= out[j + 1];
    }
    root = gfMul(root, 0x02);
  }
}
void rsRemainder(const uint8_t* data, int len, const uint8_t* div, int degree, uint8_t* out) {
  memset(out, 0, degree);
  for (int i = 0; i < len; ++i) {
    const uint8_t factor = data[i] ^ out[0];
    memmove(out, out + 1, degree - 1);
    out[degree - 1] = 0;
    for (int j = 0; j < degree; ++j) out[j] ^= gfMul(div[j], factor);
  }
}

// ---- function patterns -----------------------------------------------------
void setFn(Grid& g, Grid& fn, int x, int y, bool dark) {
  g.set(x, y, dark);
  fn.set(x, y, true);
}
void finder(Grid& g, Grid& fn, int cx, int cy) {
  for (int dy = -4; dy <= 4; ++dy)
    for (int dx = -4; dx <= 4; ++dx) {
      const int x = cx + dx, y = cy + dy;
      if (x < 0 || y < 0 || x >= g.size || y >= g.size) continue;
      const int d = dx < 0 ? (dy < 0 ? (-dx > -dy ? -dx : -dy) : (-dx > dy ? -dx : dy))
                           : (dy < 0 ? (dx > -dy ? dx : -dy) : (dx > dy ? dx : dy));
      setFn(g, fn, x, y, d != 2 && d != 4);
    }
}
void alignment(Grid& g, Grid& fn, int cx, int cy) {
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) {
      const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
      setFn(g, fn, cx + dx, cy + dy, (ax > ay ? ax : ay) != 1);
    }
}
void formatBits(Grid& g, Grid& fn, int ecc, int mask) {
  const int data = (kEccFormat[ecc] << 3) | mask;
  int rem = data;
  for (int i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
  const int bits = ((data << 10) | rem) ^ 0x5412;
  const int n = g.size;
  for (int i = 0; i <= 5; ++i) setFn(g, fn, 8, i, (bits >> i) & 1);
  setFn(g, fn, 8, 7, (bits >> 6) & 1);
  setFn(g, fn, 8, 8, (bits >> 7) & 1);
  setFn(g, fn, 7, 8, (bits >> 8) & 1);
  for (int i = 9; i < 15; ++i) setFn(g, fn, 14 - i, 8, (bits >> i) & 1);
  for (int i = 0; i < 8; ++i) setFn(g, fn, n - 1 - i, 8, (bits >> i) & 1);
  for (int i = 8; i < 15; ++i) setFn(g, fn, 8, n - 15 + i, (bits >> i) & 1);
  setFn(g, fn, 8, n - 8, true);   // the dark module
}
void functionPatterns(Grid& g, Grid& fn, int ver) {
  const int n = g.size;
  for (int i = 0; i < n; ++i) {
    setFn(g, fn, 6, i, i % 2 == 0);
    setFn(g, fn, i, 6, i % 2 == 0);
  }
  finder(g, fn, 3, 3);
  finder(g, fn, n - 4, 3);
  finder(g, fn, 3, n - 4);
  if (ver >= 2) {
    // Centres: 6, then evenly spaced to size-7 (versions up to 8 have two or three).
    int pos[3], count = ver / 7 + 2;
    pos[0] = 6;
    pos[count - 1] = n - 7;
    if (count == 3) pos[1] = (6 + n - 7) / 2;
    for (int i = 0; i < count; ++i)
      for (int j = 0; j < count; ++j) {
        if ((i == 0 && j == 0) || (i == 0 && j == count - 1) || (i == count - 1 && j == 0)) continue;
        alignment(g, fn, pos[i], pos[j]);
      }
  }
  formatBits(g, fn, 0, 0);   // reserves the area; written for real after masking
  if (ver >= 7) {
    int rem = ver;
    for (int i = 0; i < 12; ++i) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const long bits = (static_cast<long>(ver) << 12) | rem;
    for (int i = 0; i < 18; ++i) {
      const bool b = (bits >> i) & 1;
      const int a = n - 11 + i % 3, c = i / 3;
      setFn(g, fn, a, c, b);
      setFn(g, fn, c, a, b);
    }
  }
}

bool maskBit(int mask, int x, int y) {
  switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
  }
}

// The four penalty rules: long runs, 2x2 blocks, finder look-alikes, balance.
long penalty(const Grid& g) {
  const int n = g.size;
  long p = 0;
  for (int pass = 0; pass < 2; ++pass)
    for (int a = 0; a < n; ++a) {
      int run = 0;
      bool prev = false;
      for (int b = 0; b < n; ++b) {
        const bool c = pass ? g.get(a, b) : g.get(b, a);
        if (b && c == prev) ++run;
        else {
          if (run >= 5) p += 3 + run - 5;
          run = 1;
          prev = c;
        }
      }
      if (run >= 5) p += 3 + run - 5;
      for (int b = 0; b + 11 <= n; ++b) {
        static const uint8_t pat[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
        bool fwd = true, back = true;
        for (int k = 0; k < 11; ++k) {
          const bool c = pass ? g.get(a, b + k) : g.get(b + k, a);
          if (c != (pat[k] != 0)) fwd = false;
          if (c != (pat[10 - k] != 0)) back = false;
        }
        p += (fwd ? 40 : 0) + (back ? 40 : 0);
      }
    }
  int dark = 0;
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const bool c = g.get(x, y);
      dark += c;
      if (x + 1 < n && y + 1 < n && c == g.get(x + 1, y) && c == g.get(x, y + 1) && c == g.get(x + 1, y + 1)) p += 3;
    }
  const int total = n * n;
  int k = 0;
  const int diff = dark * 20 - total * 10;
  const int ad = diff < 0 ? -diff : diff;
  k = (ad + total - 1) / total - 1;
  return p + 10L * (k > 0 ? k : 0);
}

}  // namespace

bool encode(const uint8_t* data, size_t len, Code& out, int forceVersion, int forceMask, int forceEcc) {
  out.size = 0;
  int ver = 0, ecc = 0;
  for (int e = (forceEcc >= 0 ? forceEcc : 0); e <= (forceEcc >= 0 ? forceEcc : 1) && !ver; ++e)
    for (int v = (forceVersion > 0 ? forceVersion : 1); v <= (forceVersion > 0 ? forceVersion : kMaxVersion); ++v)
      // mode (4 bits) + count (8 bits) + the bytes
      if (static_cast<int>(len) <= 255 && 12 + 8 * static_cast<int>(len) <= dataCodewords(v, e) * 8) { ver = v; ecc = e; break; }
  if (!ver) return false;

  // Data codewords: byte mode, the count, the bytes, a terminator, padding.
  const int nData = dataCodewords(ver, ecc);
  uint8_t cw[242];
  memset(cw, 0, sizeof(cw));
  int bit = 0;
  auto put = [&](uint32_t v, int n) {
    for (int i = n - 1; i >= 0; --i, ++bit)
      if ((v >> i) & 1) cw[bit >> 3] |= static_cast<uint8_t>(0x80 >> (bit & 7));
  };
  put(0x4, 4);
  put(static_cast<uint32_t>(len), 8);
  for (size_t i = 0; i < len; ++i) put(data[i], 8);
  const int cap = nData * 8;
  put(0, cap - bit < 4 ? cap - bit : 4);
  bit = (bit + 7) & ~7;
  const int pads = (cap - bit) / 8;
  for (int k = 0; k < pads; ++k) put(k % 2 ? 0x11 : 0xEC, 8);

  // Split into blocks, add each block's error correction, interleave.
  const int blocks = kBlocks[ecc][ver], eccLen = kEccPerBlock[ecc][ver];
  const int raw = rawDataModules(ver) / 8;
  const int shortBlocks = blocks - raw % blocks, shortLen = raw / blocks;
  uint8_t div[30], eccBuf[4][30], final[242];
  rsDivisor(eccLen, div);
  int starts[4], lens[4];
  for (int i = 0, k = 0; i < blocks; ++i) {
    lens[i] = shortLen - eccLen + (i < shortBlocks ? 0 : 1);
    starts[i] = k;
    rsRemainder(cw + k, lens[i], div, eccLen, eccBuf[i]);
    k += lens[i];
  }
  int o = 0;
  for (int i = 0; i <= shortLen - eccLen; ++i)
    for (int b = 0; b < blocks; ++b)
      if (i < lens[b]) final[o++] = cw[starts[b] + i];
  for (int i = 0; i < eccLen; ++i)
    for (int b = 0; b < blocks; ++b) final[o++] = eccBuf[b][i];

  // Function patterns, then the codewords in the zigzag order.
  const int n = 4 * ver + 17;
  uint8_t fnBits[kBitmapBytes], baseBits[kBitmapBytes];
  memset(fnBits, 0, sizeof(fnBits));
  memset(baseBits, 0, sizeof(baseBits));
  Grid base = {n, baseBits}, fn = {n, fnBits};
  functionPatterns(base, fn, ver);
  int i = 0;
  for (int right = n - 1; right >= 1; right -= 2) {
    if (right == 6) right = 5;
    for (int vert = 0; vert < n; ++vert)
      for (int j = 0; j < 2; ++j) {
        const int x = right - j;
        const bool up = ((right + 1) & 2) == 0;
        const int y = up ? n - 1 - vert : vert;
        if (!fn.get(x, y) && i < raw * 8) {
          base.set(x, y, (final[i >> 3] >> (7 - (i & 7))) & 1);
          ++i;
        }
      }
  }

  // Every mask on a copy, keeping the one with the lowest penalty.
  out.size = n;
  Grid g = {n, out.bits};
  long best = -1;
  int bestMask = 0;
  for (int m = 0; m < 8; ++m) {
    if (forceMask >= 0 && m != forceMask) continue;
    memcpy(out.bits, baseBits, sizeof(baseBits));
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x)
        if (!fn.get(x, y) && maskBit(m, x, y)) g.set(x, y, !g.get(x, y));
    formatBits(g, fn, ecc, m);
    const long p = penalty(g);
    if (best < 0 || p < best) { best = p; bestMask = m; }
  }
  memcpy(out.bits, baseBits, sizeof(baseBits));
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x)
      if (!fn.get(x, y) && maskBit(bestMask, x, y)) g.set(x, y, !g.get(x, y));
  formatBits(g, fn, ecc, bestMask);
  return true;
}

bool wifiPayload(const char* ssid, const char* pass, char* out, size_t cap) {
  size_t n = 0;
  auto add = [&](const char* s, bool escape) {
    for (; s && *s; ++s) {
      if (escape && strchr("\\;,:\"", *s)) {
        if (n + 1 >= cap) return false;
        out[n++] = '\\';
      }
      if (n + 1 >= cap) return false;
      out[n++] = *s;
    }
    return true;
  };
  const bool open = !(pass && pass[0]);
  const bool ok = add(open ? "WIFI:T:nopass;S:" : "WIFI:T:WPA;S:", false) && add(ssid, true) &&
                  (open || (add(";P:", false) && add(pass, true))) && add(";;", false);
  if (cap) out[ok ? n : 0] = 0;
  return ok;
}

}  // namespace qr
