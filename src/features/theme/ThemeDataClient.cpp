#include "ThemeDataClient.h"
#include "Platform.h"
#include <ArduinoJson.h>
#include <new>

namespace {
struct Deadline {
  gm::DataFetch& request;
  uint32_t started = millis();
  explicit Deadline(gm::DataFetch& r) : request(r) {}
  bool expired() const {
    return request.cancelled() || uint32_t(millis() - started) >= gm::DataTimeoutMs;
  }
};

// HTTPClient's timeout is an idle timeout, including for header parsing. Stop
// its transport at the overall deadline too, so trickling headers cannot keep
// the worker alive indefinitely. No other task touches this socket.
template <class Transport>
class BoundedClient : public Transport {
 public:
  explicit BoundedClient(Deadline& deadline) : deadline_(deadline) {}
  using Transport::connect;
  // ESP8266's Client has no connect(host,port,timeout) overload; allowed()
  // already bounds every socket call regardless, so the missing explicit
  // connect timeout costs nothing there.
  int connect(const char* host, uint16_t port) override {
    if (!allowed()) return 0;
    int result = Transport::connect(host, port);
    return allowed() ? result : 0;
  }
  int available() override { return allowed() ? Transport::available() : 0; }
  uint8_t connected() override { return allowed() ? Transport::connected() : 0; }
  int read() override { return allowed() ? Transport::read() : -1; }
  int read(uint8_t* buffer, size_t count) override { return allowed() ? Transport::read(buffer, count) : -1; }
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* buffer, size_t count) override { return allowed() ? Transport::write(buffer, count) : 0; }
 private:
  bool allowed() {
    if (!deadline_.expired()) return true;
    Transport::stop();
    // Also end Stream::readStringUntil's internal timedRead after cancellation.
    this->setTimeout(1);
    return false;
  }
  Deadline& deadline_;
};

// After GET(), read the body through HTTPClient's stream (the same pattern
// StockClient and OtaUpdate use): HTTPClient owns the response from that
// point, and on ESP8266 the raw client behind it never yields body bytes, so
// every theme-data fetch failed at the body-read stage despite HTTP 200.
struct BodyStream {
  Stream& stream;
  NetClient& client;
  Deadline& deadline;
  uint32_t now() const { return millis(); }
  bool cancelled() const { return deadline.expired(); }
  int available() { return stream.available(); }
  bool connected() { return client.connected(); }
  int read(uint8_t* buffer, size_t count) { return (int)stream.readBytes(buffer, count); }
  void idle() { delay(1); }
};

std::string jsonValue(JsonVariantConst value, int decimals) {
  if (value.isNull()) return "--";
  if (value.is<const char*>()) {
    std::string s = value.as<const char*>();
    if (s.size() > 31) {
      // Cut on a UTF-8 character boundary so Cyrillic is not left half a letter.
      size_t cut = 31;
      while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xc0) == 0x80) --cut;
      s.resize(cut);
    }
    return s;
  }
  if (!value.is<bool>() && !value.is<int>() && !value.is<long>() && !value.is<float>() && !value.is<double>()) return "--";
  if (decimals >= 0 && !value.is<bool>()) {   // a rounded number, never "-0"
    char out[24];
    snprintf(out, sizeof(out), "%.*f", decimals, value.as<double>());
    bool zero = true;
    for (const char* p = out; *p; ++p) if (*p >= '1' && *p <= '9') zero = false;
    return std::string(zero && out[0] == '-' ? out + 1 : out);
  }
  char encoded[40] = {};
  size_t n = serializeJson(value, encoded, sizeof(encoded) - 1);
  encoded[n] = 0;
  if (n >= sizeof(encoded) - 1) return "--";
  return std::string(encoded);
}
JsonVariantConst jsonPath(JsonVariantConst root, const std::string& path) {
  size_t start = 0;
  while (start < path.size()) {
    size_t dot = path.find('.', start);
    std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
    if (part.empty() || !root.is<JsonObjectConst>()) return JsonVariantConst();
    root = root[part.c_str()];
    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  return root;
}
bool fetchThemeSource(gm::DataFetch& request) {
  if (request.cancelled()) return false;
  Deadline deadline(request);
  std::unique_ptr<NetClient> client;
  if (request.source.url.rfind("https://", 0) == 0) {
    return false;   // no TLS in this build (config.h): the source shows "--"
    if (ESP.getFreeHeap() < 18000) return false;
    auto secure = new(std::nothrow) BoundedClient<SecureClient>(deadline);
    if (!secure) return false;
    // HTTPS reaches this path only after the manifest explicitly opted in.
    secure->setInsecure();
    client.reset(secure);
  } else client.reset(new(std::nothrow) BoundedClient<WiFiClient>(deadline));
  if (!client) return false;
  // ESP8266's WiFiClient/HTTPClient have no separate connect-phase timeout;
  // Deadline (via BoundedClient::allowed()) already bounds every socket call
  // on every platform, so these are ESP32-only belt-and-suspenders.
  HTTPClient http;
  http.setTimeout(gm::DataTimeoutMs);
  http.setReuse(false);
  http.useHTTP10(true);
  if (!http.begin(*client, request.source.url.c_str())) return false;
  http.addHeader("Accept", "application/json");
  if (http.GET() != HTTP_CODE_OK || deadline.expired()) return false;
  std::string body;
  BodyStream stream{http.getStream(), *client, deadline};
  bool complete = gm::readDataBody(stream, http.getSize(), body);
  http.end();
  if (!complete || request.cancelled()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::NestingLimit(8))) return false;
  request.values.reserve(request.source.fields.size());
  for (const auto& field : request.source.fields) {
    request.values.push_back({request.source.id + "." + field.id, jsonValue(jsonPath(doc.as<JsonVariantConst>(), field.path), field.decimals)});
  }
  return !request.cancelled();
}
}

// No FreeRTOS on this chip: run the bounded fetch synchronously on the
// caller's own stack, the same "one blocking call per main-loop tick"
// pattern the ticker and radar clients already use here. Deadline (inside
// fetchThemeSource) already bounds this to DataTimeoutMs plus body-read
// time, the same order of magnitude a TLS handshake already costs this
// chip elsewhere.
bool startThemeDataFetch(const std::shared_ptr<gm::DataFetch>& request) {
  request->finish(fetchThemeSource(*request));
  return true;
}
