// Prints the firmware's QR matrices for tests/screens/test_qr.py. Each input
// line is "version mask ecc payload" (-1 = let the encoder pick); each output
// is the module rows as 0/1 strings, then a blank line ("-" if it failed).
#include "QrCode.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    int ver, mask, ecc, used = 0;
    if (std::sscanf(line.c_str(), "%d %d %d %n", &ver, &mask, &ecc, &used) < 3) continue;
    const std::string payload = line.substr(used);
    qr::Code c;
    if (!qr::encode(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), c, ver, mask, ecc)) {
      std::cout << "-\n\n";
      continue;
    }
    for (int y = 0; y < c.size; ++y) {
      for (int x = 0; x < c.size; ++x) std::cout << (c.dark(x, y) ? '1' : '0');
      std::cout << '\n';
    }
    std::cout << '\n';
  }
}
