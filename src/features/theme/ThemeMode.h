#pragma once
#include "config.h"
#include "Mode.h"
#include "ThemePackage.h"
#include "ThemeData.h"
#include <LittleFS.h>
#include <memory>
#include <vector>

class ThemeFile : public gm::Source {
 public:
  bool open(const String& path) {
    file_ = LittleFS.open(path, "r");
    return bool(file_);
  }
  uint32_t size() const override { return file_ ? file_.size() : 0; }
  bool read(uint32_t offset, void* dest, size_t count) override {
    return file_ && file_.seek(offset) && file_.read(static_cast<uint8_t*>(dest), count) == count;
  }
  void pause() override { yield(); }
 private:
  File file_;
};
class ThemeMode : public DisplayMode {
 public:
  const char* id() const override { return "theme"; }
  uint8_t modeConst() const override { return MODE_THEME; }
  void begin(const Settings&) override;
  void service(const Settings&) override;
  void invalidate(const Settings&) override;
  void wake(const Settings&) override {
    engine_.invalidate();
    messageDrawn_ = false;
  }
  const String& error() const { return error_; }
  // The running theme's fetched values, and how fresh they are: seconds since
  // the last good fetch (-1: none yet) and failed fetches since then.
  const std::vector<gm::ThemeValue>& dataValues() const { return dataValues_; }
  // What the firmware handed the face (device.*, e.g. the weather).
  const std::vector<gm::ThemeValue>& deviceValues() const { return engine_.deviceValues(); }
  int32_t dataAge() const { return dataOkAt_ ? (int32_t)((millis() - dataOkAt_) / 1000) : -1; }
  uint16_t dataFails() const { return dataFails_; }
  // What parsing a package needs: its peak heap (tests/theme/test_memory.cpp
  // holds the examples to 5 KB) plus about 1 KB that the network code may
  // claim meanwhile, and blocks of up to 1 KB (the same test's
  // largest-request limit) with some slack.
  static constexpr uint32_t ParseFreeHeap = 6144;
  static constexpr uint32_t ParseLargestBlock = 1536;
  // Frees the loaded theme when the heap is short of what a web request needs,
  // so the request does not run out of memory next to it; on the ESP8266 a
  // theme and a second package parse do not fit together. With
  // enough heap it does nothing. The selection, the fetched values and the
  // fetch schedule stay: the next service() call loads the theme again and
  // repaints it with the same values, and no source is fetched early.
  void release(uint32_t needFree = ParseFreeHeap, uint32_t needLargestBlock = ParseLargestBlock);
  // DisplayMode::freeForRequest(), before a settings read, save or import.
  // A save needs about 7 KB of heap (a measurement on a SmallTV-Ultra)
  // plus the same 1 KB of network slack as ParseFreeHeap, and builds a
  // 2.4 KB settings document. On the ESP8266 with a
  // theme running that is not free, so the theme is released; on the ESP32
  // boards it always is, and the theme stays.
  static constexpr uint32_t RequestFreeHeap = 8192;
  static constexpr uint32_t RequestLargestBlock = 3072;
  void freeForRequest() override { release(RequestFreeHeap, RequestLargestBlock); }
 private:
  void unload();
  void refreshData();
  gm::Engine engine_;
  std::unique_ptr<ThemeFile> file_;
  std::unique_ptr<gm::Package> package_;
  String loadedId_, error_;
  std::vector<uint32_t> dataNextMs_;
  std::vector<bool> dataScheduled_;
  std::vector<gm::ThemeValue> dataValues_;
  gm::DataRequests dataRequests_;
  size_t dataCursor_ = 0;
  uint32_t dataOkAt_ = 0;      // millis() of the last good fetch, 0: none
  uint16_t dataFails_ = 0;     // failed fetches since it
  // Set by release(): the next load of the same theme keeps the data state.
  bool keepData_ = false;
  bool attempted_ = false, messageDrawn_ = false;
  // The face shows device.weather* (the Weather app's reading): then the
  // reading is kept fresh while the face is on the screen, and handed to the
  // engine whenever it, or the language, changes (0: not handed yet).
  bool usesWeather_ = false;
  uint32_t weatherSeen_ = 0;
  void weatherValues(const Settings& s, uint8_t lang);
  void installDefaultFaces();
};
extern ThemeMode g_themeMode;
