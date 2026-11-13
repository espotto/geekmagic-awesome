// Writes 240x240 RGB565 frames side by side as a binary PPM, for a visual check.
#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
inline void writeFrames(const char* path, const std::vector<std::vector<uint16_t>>& frames) {
  const int n = static_cast<int>(frames.size()), gap = 12, W = n * 240 + (n - 1) * gap;
  std::vector<unsigned char> out(W * 240 * 3, 60);
  for (int i = 0; i < n; ++i)
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x) {
        const uint16_t c = frames[i][y * 240 + x];
        unsigned char* o = &out[(y * W + i * (240 + gap) + x) * 3];
        o[0] = ((c >> 11) * 255) / 31;
        o[1] = (((c >> 5) & 63) * 255) / 63;
        o[2] = ((c & 31) * 255) / 31;
      }
  FILE* f = std::fopen(path, "wb");
  assert(f);
  std::fprintf(f, "P6\n%d %d\n255\n", W, 240);
  std::fwrite(out.data(), 1, out.size(), f);
  std::fclose(f);
}
