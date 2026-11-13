#include "RemindersMode.h"
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include "AtomicJson.h"
#include "Gfx.h"
#include "RemindersView.h"
#include "../theme/Languages.h"

RemindersMode g_remindersMode;

static const char kPath[] = "/reminders.json";
static const char kTemp[] = "/reminders.tmp";

void RemindersMode::begin(const Settings&) { load(); }

void RemindersMode::load() {
  list_.clear();
  File f = LittleFS.open(kPath, "r");
  if (!f) return;
  JsonDocument doc;
  if (!deserializeJson(doc, f))
    for (JsonObjectConst it : doc["items"].as<JsonArrayConst>()) {
      if (!list_.add(it["text"] | "")) continue;
      list_.setDone(list_.count() - 1, it["done"] | false);
    }
  f.close();
}

bool RemindersMode::save() {
  JsonDocument doc;
  JsonArray items = doc["items"].to<JsonArray>();
  for (int i = 0; i < list_.count(); i++) {
    JsonObject o = items.add<JsonObject>();
    o["text"] = list_.at(i).text;
    o["done"] = list_.at(i).done;
  }
  return saveJsonAtomically(LittleFS, doc, kPath, kTemp);
}

void RemindersMode::toJson(JsonObject out) const {
  out["open"] = list_.open();
  out["max"] = reminders::kMax;
  JsonArray items = out["items"].to<JsonArray>();
  for (int i = 0; i < list_.count(); i++) {
    JsonObject o = items.add<JsonObject>();
    o["text"] = list_.at(i).text;
    o["done"] = list_.at(i).done;
  }
}

int RemindersMode::apply(JsonVariantConst body, String& error) {
  if (!body.is<JsonObjectConst>()) { error = "expected a JSON object"; return 400; }
  reminders::List next = list_;   // all or nothing: a failed step changes nothing
  bool any = false;
  auto index = [&](const char* key, int& i) {
    if (body[key].isNull()) return false;
    any = true;
    i = body[key] | -1;
    if (!body[key].is<int>() || i < 0 || i >= next.count()) { error = String(key) + ": no such item"; return true; }
    return false;
  };
  if (body["items"].is<JsonArrayConst>()) {
    any = true;
    next.clear();
    for (JsonVariantConst it : body["items"].as<JsonArrayConst>()) {
      const char* text = it.is<const char*>() ? it.as<const char*>() : (it["text"] | "");
      if (!next.add(text)) { error = next.count() == reminders::kMax ? "list full" : "empty item"; return 400; }
      if (it["done"] | false) next.setDone(next.count() - 1, true);
    }
  }
  if (!body["add"].isNull()) {
    any = true;
    JsonVariantConst add = body["add"];
    if (add.is<JsonArrayConst>()) {
      for (JsonVariantConst t : add.as<JsonArrayConst>())
        if (!next.add(t | "")) { error = next.count() == reminders::kMax ? "list full" : "empty item"; return next.count() == reminders::kMax ? 409 : 400; }
    } else if (!next.add(add | "")) {
      error = next.count() == reminders::kMax ? "list full" : "empty item";
      return next.count() == reminders::kMax ? 409 : 400;
    }
  }
  int i;
  if (index("done", i)) return 400;
  if (!body["done"].isNull()) next.setDone(i, true);
  if (index("undone", i)) return 400;
  if (!body["undone"].isNull()) next.setDone(i, false);
  if (index("remove", i)) return 400;
  if (!body["remove"].isNull()) next.remove(i);
  if (body["clearDone"] | false) { any = true; next.clearDone(); }
  if (body["clear"] | false) { any = true; next.clear(); }
  if (!any) { error = "nothing to do: add, done, undone, remove, clearDone, clear or items"; return 400; }
  list_ = next;
  version_++;
  if (!save()) { error = "could not save to flash"; return 500; }
  return 200;
}

void RemindersMode::service(const Settings& s) {
  const uint8_t lang = gm::languageFromCode(s.clock.lang.c_str());
  const uint32_t key = version_ * 8 + lang;
  if (key == drawn_) return;
  drawn_ = key;
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  remindersview::Layout l;
  remindersview::layout(list_, lang, l);
  gfx->fillScreen(C_BLACK);
  uint16_t row[remindersview::kWidth];
  for (int y = remindersview::kSafe - 4; y < remindersview::kHeight - remindersview::kSafe + 4; y++) {
    remindersview::renderRow(l, y, row);
    for (int x = 0; x < remindersview::kWidth; x++) row[x] = gfxTint(row[x]);
    gfx->draw16bitRGBBitmap(0, y, row, remindersview::kWidth, 1);
    if ((y & 15) == 15) yield();
  }
}
