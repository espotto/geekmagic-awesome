// c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc tests/test_webguard.cpp -o /tmp/geekmagic-webguard-tests
// && /tmp/geekmagic-webguard-tests
#include "WebGuard.h"
#include <cstdio>
#include <cstdlib>

using namespace webguard;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void testHost() {
  const char* hn = "geekmagic";
  // No Host header (HTTP/1.0)
  CHECK(hostAllowed(nullptr, hn));
  CHECK(hostAllowed("", hn));
  // IPv4 literals, with and without a port
  CHECK(hostAllowed("192.168.1.50", hn));
  CHECK(hostAllowed("192.168.4.1", hn));
  CHECK(hostAllowed("10.0.0.2:80", hn));
  CHECK(hostAllowed("192.168.1.50:8080", hn));
  CHECK(!hostAllowed("192.168.1.256", hn));
  CHECK(!hostAllowed("192.168.1", hn));
  CHECK(!hostAllowed("192.168.1.1.1", hn));
  CHECK(hostAllowed("192.168.1.1.", hn));   // one trailing dot is the same name
  CHECK(!hostAllowed("1.2.3.4:", hn));
  CHECK(!hostAllowed("1.2.3.4:80x", hn));
  CHECK(!hostAllowed("1.2.3.4:123456", hn));
  // The device's own name
  CHECK(hostAllowed("geekmagic", hn));
  CHECK(hostAllowed("geekmagic:80", hn));
  CHECK(hostAllowed("geekmagic.local", hn));
  CHECK(hostAllowed("GeekMagic.LOCAL", hn));
  CHECK(hostAllowed("geekmagic.local.", hn));
  CHECK(hostAllowed("geekmagic.lan", hn));
  CHECK(hostAllowed("geekmagic.home", hn));
  CHECK(hostAllowed("geekmagic.home.arpa", hn));
  CHECK(hostAllowed("geekmagic.internal", hn));
  CHECK(hostAllowed("geekmagic.localdomain", hn));
  CHECK(hostAllowed("geekmagic.fritz.box", hn));
  CHECK(hostAllowed("geekmagic.fritz.box:80", hn));
  CHECK(hostAllowed("geekmagic-desk.local", "geekmagic-desk"));
  CHECK(hostAllowed("geekmagic-a1b2", "geekmagic-a1b2"));
  // Rebinding candidates
  CHECK(!hostAllowed("geekmagic.attacker.com", hn));
  CHECK(!hostAllowed("geekmagic.local.attacker.com", hn));
  CHECK(!hostAllowed("geekmagic.lan.evil.example", hn));
  CHECK(!hostAllowed("geekmagic.com", hn));
  CHECK(!hostAllowed("attacker.com", hn));
  CHECK(!hostAllowed("evilgeekmagic.local", hn));
  CHECK(!hostAllowed("geekmagicx.local", hn));
  CHECK(!hostAllowed("geekmagic.localx", hn));
  CHECK(hostAllowed("geekmagic.", "geekmagic"));   // bare name with a trailing dot
  CHECK(!hostAllowed("localhost", hn));
  CHECK(!hostAllowed("geekmagic.local:", hn));
  CHECK(!hostAllowed(".", hn));
  CHECK(!hostAllowed(":80", hn));
  CHECK(!hostAllowed("attacker.com", ""));
  CHECK(!hostAllowed("attacker.com", nullptr));
  CHECK(hostAllowed("192.168.1.50", ""));
}

static void testOrigin() {
  const char* h = "geekmagic.local";
  // Not a browser: neither header
  CHECK(originAllowed(nullptr, nullptr, h));
  CHECK(originAllowed("", "", h));
  CHECK(originAllowed(nullptr, nullptr, nullptr));
  // The web UI itself
  CHECK(originAllowed("http://geekmagic.local", nullptr, h));
  CHECK(originAllowed("http://geekmagic.local", "same-origin", h));
  CHECK(originAllowed("http://GeekMagic.Local", nullptr, h));
  CHECK(originAllowed("http://192.168.1.50", "same-origin", "192.168.1.50"));
  CHECK(originAllowed("http://192.168.1.50:8080", nullptr, "192.168.1.50:8080"));
  // Address-bar navigation / bookmarks
  CHECK(originAllowed(nullptr, "none", h));
  // Cross-site
  CHECK(!originAllowed("http://attacker.com", nullptr, h));
  CHECK(!originAllowed("http://attacker.com", "cross-site", h));
  CHECK(!originAllowed(nullptr, "cross-site", h));
  CHECK(!originAllowed(nullptr, "same-site", h));
  CHECK(!originAllowed("http://geekmagic.local", "cross-site", h));
  CHECK(!originAllowed("null", nullptr, h));
  CHECK(!originAllowed("null", "same-origin", h));
  // Port must match
  CHECK(!originAllowed("http://geekmagic.local:8080", nullptr, h));
  CHECK(!originAllowed("http://geekmagic.local", nullptr, "geekmagic.local:8080"));
  // Scheme must be http, host must match whole
  CHECK(!originAllowed("https://geekmagic.local", nullptr, h));
  CHECK(!originAllowed("http://geekmagic.local.attacker.com", nullptr, h));
  CHECK(!originAllowed("http://geekmagic.localx", nullptr, h));
  CHECK(!originAllowed("http://", nullptr, h));
  CHECK(!originAllowed("http://geekmagic.local/", nullptr, h));
  // Origin but no Host to compare against
  CHECK(!originAllowed("http://geekmagic.local", nullptr, nullptr));
  CHECK(!originAllowed("http://geekmagic.local", nullptr, ""));
}

int main() {
  testHost();
  testOrigin();
  if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
  std::printf("webguard: all tests passed\n");
  return 0;
}
