// Writes the system font blob that GET /api/font serves (writeSystemFont) to
// the file named on the command line, for the web UI preview tests.
#include "SystemFont.h"
#include <cstdio>
static void emit(void* f, const uint8_t* data, size_t n) { std::fwrite(data, 1, n, static_cast<FILE*>(f)); }
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  FILE* f = std::fopen(argv[1], "wb");
  if (!f) return 1;
  const size_t n = gm::writeSystemFont(emit, f);
  std::fclose(f);
  return n == gm::writeSystemFont(nullptr, nullptr) ? 0 : 1;
}
