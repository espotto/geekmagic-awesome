#include "ThemeMode.h"
#include "Clock.h"
#include "Gfx.h"
#include "Platform.h"
#include "ThemeDataClient.h"
#include "SysText.h"
#include "WeatherMode.h"
#include <Arduino_GFX_Library.h>
#include <algorithm>

ThemeMode g_themeMode;
namespace {
// Adapts the portable engine's Display interface to this device's actual
// panel: row() applies the usual panel color tint before handing the row to the
// GFX driver. yield() every 16 rows keeps a large dirty region from starving
// WiFi/other tasks during a long render.
class Panel : public gm::Display {
 public:
  void row(int x, int y, const uint16_t* pixels, int count) override {
    uint16_t tinted[240];
    for (int i = 0; i < count; ++i) tinted[i] = gfxTint(pixels[i]);
    gfxDev()->draw16bitRGBBitmap(x, y, tinted, count, 1);
    if ((y & 15) == 15) yield();
  }
};
// What a load needs before it starts: a package parse (ThemeMode.h). A failed
// allocation mid-load restarts the ESP8266, so a load that would not fit
// waits instead.
constexpr uint32_t LoadFreeHeap = ThemeMode::ParseFreeHeap;
constexpr uint32_t LoadLargestBlock = ThemeMode::ParseLargestBlock;
const char* const WaitingForMemory = "Waiting for free memory";
}
// Whether a face reads any device.weather* value: a text token in any
// language, or a binding source.
static bool usesWeather(const gm::Theme& t) {
  static const char kKey[] = "device.weather";
  for (const auto& l : t.layers) {
    if (l.value.find(kKey) != std::string::npos) return true;
    for (const auto& b : l.bindings) if (b.source.compare(0, sizeof(kKey) - 1, kKey) == 0) return true;
  }
  for (const auto& tr : t.translations) if (tr.text.find(kKey) != std::string::npos) return true;
  return false;
}

// The Weather app's reading as device.weather*: whole degrees C, humidity in
// %, wind in m/s, the WMO code, the condition in the device language, 1 by
// day. Nothing until the first reading: the face shows "--" meanwhile.
void ThemeMode::weatherValues(const Settings& s, uint8_t lang) {
  g_weatherMode.keepFresh(s);
  const uint32_t seen = g_weatherMode.version() * 4 + lang + 1;
  if (seen == weatherSeen_) return;
  weatherSeen_ = seen;
  const weatherview::Data& d = g_weatherMode.data();
  std::vector<gm::ThemeValue> v;
  if (d.valid) {
    auto whole = [](float x) { return (long)(x < 0 ? x - 0.5f : x + 0.5f); };
    const long nums[] = {whole(d.temp), whole(d.feels), whole(d.high), whole(d.low),
                         (long)d.humidity, whole(d.wind / 3.6f), (long)d.code};
    // The names after "device.weather", in flash (on the ESP8266 a literal is RAM).
    static const char kNames[] PROGMEM = "Temp|Feel|High|Low|Hum|Wind|Code|Text|Day";
    std::string key = "device.weather";
    const size_t base = key.size();
    size_t i = 0;
    for (const char* p = kNames;; ++p) {
      const char c = (char)pgm_read_byte(p);
      if (c && c != '|') { key += c; continue; }
      char val[48];
      if (i < 7) snprintf(val, sizeof(val), "%ld", nums[i]);
      else if (i == 7) weatherview::conditionText(d.code, lang, val, sizeof(val));
      else snprintf(val, sizeof(val), "%d", d.day ? 1 : 0);
      v.push_back({key, val});
      key.resize(base);
      ++i;
      if (!c) break;
    }
  }
  engine_.setDeviceValues(std::move(v));
}

void ThemeMode::begin(const Settings&) {
  LittleFS.mkdir("/themes");
  // An interrupted install leaves its staging file behind; boot is the one
  // point it is safe to remove without racing a request in progress.
  LittleFS.remove("/themes/.upload");
}
// Drops every bit of state for the currently loaded theme -- the open
// package/file, any in-flight or fetched data, and the engine itself -- so
// the next service() call starts a clean load from scratch.
void ThemeMode::unload() {
  dataRequests_.cancel();
  dataRequests_.take();
  package_.reset();
  file_.reset();
  dataNextMs_.clear();
  dataScheduled_.clear();
  dataValues_.clear();
  dataCursor_ = 0;
  dataOkAt_ = 0;
  dataFails_ = 0;
  engine_.setValues({});
  engine_.setTheme(gm::Theme{}, millis());
  loadedId_ = "";
  attempted_ = messageDrawn_ = keepData_ = false;
  usesWeather_ = false;
  weatherSeen_ = 0;
  error_ = "";
}
void ThemeMode::release(uint32_t needFree, uint32_t needLargestBlock) {
  if (!package_ || (ESP.getFreeHeap() >= needFree && platformMaxFreeBlock() >= needLargestBlock)) return;
  // A fetch in flight is dropped; its source is still due, so the reload
  // fetches it again.
  dataRequests_.cancel();
  dataRequests_.take();
  package_.reset();
  file_.reset();
  engine_.setValues({});
  engine_.setTheme(gm::Theme{}, millis());
  attempted_ = messageDrawn_ = false;
  keepData_ = true;
}
// Collects a finished fetch (if any) and, with at most one request in flight
// at a time, starts the next source whose interval has elapsed, round-robin
// over sources so no single slow/failing one starves the others.
void ThemeMode::refreshData() {
  const auto& sources = engine_.theme().data;
  if (auto result = dataRequests_.take()) {
    if (result->success) {
      // Merge into the running set by key rather than replace it wholesale:
      // a source's fields persist across refreshes of the *other* sources, so
      // a slow source does not blank fields a fast one already delivered.
      for (auto& value : result->values) {
        auto existing = std::find_if(dataValues_.begin(), dataValues_.end(), [&](const gm::ThemeValue& old) { return old.key == value.key; });
        if (existing == dataValues_.end()) dataValues_.push_back(std::move(value));
        else existing->value = std::move(value.value);
      }
      engine_.setValues(dataValues_);
      dataOkAt_ = millis();
      if (!dataOkAt_) dataOkAt_ = 1;
      dataFails_ = 0;
    } else if (dataFails_ < 65535) {
      dataFails_++;
    }
    // Schedule from completion, preserving the interval even for slow sources.
    dataNextMs_[result->index] = millis() + sources[result->index].interval * 1000UL;
  }
  if (sources.empty() || dataRequests_.busy() || WiFi.status() != WL_CONNECTED) return;
  // Scan forward from `dataCursor_` for the first source that is due (an
  // unscheduled one always is), start it, and advance the cursor past it --
  // so a source that just fired goes to the back of the round-robin queue.
  for (size_t offset = 0; offset < sources.size(); ++offset) {
    size_t i = (dataCursor_ + offset) % sources.size();
    if (dataScheduled_[i] && static_cast<int32_t>(millis() - dataNextMs_[i]) < 0) continue;
    auto request = dataRequests_.start(sources[i], i);
    if (!startThemeDataFetch(request)) request->finish(false);
    dataScheduled_[i] = true;
    dataCursor_ = (i + 1) % sources.size();
    break;
  }
}
// Called when the theme should be redrawn from scratch, e.g. when a
// notification overlay just finished. Switches to a different theme by
// unloading first; otherwise leaves the loaded package alone and only marks
// the engine's next update() as a full repaint.
void ThemeMode::invalidate(const Settings& s) {
  const bool onScreen = s.mode == MODE_THEME || (s.mode == MODE_CAROUSEL && s.carouselTheme);
  if (loadedId_ != s.themeId || !onScreen) unload();
  // A failed load may be retried after installation or settings changes.
  if (!package_) attempted_ = false;
  engine_.invalidate();
  messageDrawn_ = false;
}
// Per-frame entry point: loads the selected theme at most once per selection
// (via `attempted_`, so a failed load is not retried every call -- only on
// the next invalidate()/selection change), then renders it. `attempted_` and
// `loadedId_` together are the load state machine; everything past the load
// block runs every call once a package is open.
void ThemeMode::service(const Settings& s) {
  if (loadedId_ != s.themeId) {
    unload();
    loadedId_ = s.themeId;
  }
  // A web request, or the buffers of its reply, can briefly hold the heap a
  // load needs; the load then waits, checked again every frame.
  const bool loadFits = ESP.getFreeHeap() >= LoadFreeHeap && platformMaxFreeBlock() >= LoadLargestBlock;
  if (!attempted_ && !loadFits && error_ != WaitingForMemory) {
    error_ = WaitingForMemory;
    messageDrawn_ = false;
  }
  if (!attempted_ && loadFits) {
    attempted_ = true;
    std::string error;
    gm::Theme theme;
    if (!gm::validId(s.themeId.c_str())) error = "Select an installed theme";
    else {
      file_.reset(new ThemeFile);
      if (!file_->open("/themes/" + s.themeId + ".stheme")) error = "Theme file not found";
      else {
        package_.reset(new gm::Package(*file_));
        if (!package_->load(theme, error)) package_.reset();
        else if (theme.id != s.themeId.c_str()) {
          error = "Theme ID mismatch";
          package_.reset();
        }
      }
    }
    if (!error.empty()) {
      error_ = error.c_str();
      package_.reset();
      file_.reset();
    } else {
      // After a release the same package loads again, so its sources match.
      if (!keepData_ || dataNextMs_.size() != theme.data.size()) {
        dataNextMs_.assign(theme.data.size(), 0);
        dataScheduled_.assign(theme.data.size(), false);
        dataValues_.clear();
        dataCursor_ = 0;
        dataOkAt_ = 0;
        dataFails_ = 0;
      }
      usesWeather_ = usesWeather(theme);
      weatherSeen_ = 0;
      engine_.setTheme(std::move(theme), millis());
      engine_.setValues(dataValues_);
      error_ = "";
    }
  }
  if (!package_) {
    if (!messageDrawn_) {
      // The title and the known reasons in the device language; a package's own
      // check (which layer, which field) stays as written.
      static const sys::Word kWords[] = {sys::FaceWaitingMemory, sys::FaceSelect, sys::FaceFileMissing,
                                         sys::FaceIdMismatch, sys::FaceAssetError};
      const char* const known[] = {WaitingForMemory, "Select an installed theme", "Theme file not found",
                                   "Theme ID mismatch", "Could not read theme asset"};
      char title[32], why[64];
      const char* text = error_.c_str();
      for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); i++)
        if (error_ == known[i]) text = sys::text(kWords[i], why, sizeof(why));
      gfxMessage(sys::text(sys::WatchFace, title, sizeof(title)), text, C_WHITE);
      messageDrawn_ = true;
    }
    return;
  }
  refreshData();
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  if (usesWeather_) weatherValues(s, lang);
  tm t{};
  bool valid = clockNow(t);
  Panel panel;
  engine_.setLanguage(lang);
  auto dirty = engine_.update(millis(), valid ? &t : nullptr);
  // A render failure (a corrupt or truncated asset read mid-frame) is treated
  // like a load failure: drop the package and show an error rather than
  // risk repeatedly failing on every subsequent frame.
  if (!gm::render(engine_, dirty, *package_, panel)) {
    dataRequests_.cancel();
    dataRequests_.take();
    error_ = "Could not read theme asset";
    package_.reset();
    file_.reset();
    messageDrawn_ = false;
  }
}
