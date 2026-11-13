// RemindersList.h — the Reminders app's list: up to kMax short items, each
// done or not, kept in order of arrival. Plain data and rules, free of
// Arduino, so the desktop tests it (tests/reminders); RemindersMode stores it
// in flash and serves it over HTTP.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace reminders {

constexpr int kMax = 12;        // items the list holds
constexpr int kTextMax = 63;    // bytes of UTF-8 per item

struct Item {
  char text[kTextMax + 1];
  bool done;
};

class List {
 public:
  int count() const { return count_; }
  const Item& at(int i) const { return items_[i]; }
  int open() const {
    int n = 0;
    for (int i = 0; i < count_; ++i) n += !items_[i].done;
    return n;
  }
  // Adds `text` (trimmed, cut at a whole UTF-8 character) at the end. A full
  // list makes room by dropping its oldest done item; false when every item
  // is still open, or the text is empty.
  bool add(const char* text) {
    char t[kTextMax + 1];
    if (!clean(text, t)) return false;
    if (count_ == kMax) {
      int drop = -1;
      for (int i = 0; i < count_ && drop < 0; ++i) if (items_[i].done) drop = i;
      if (drop < 0) return false;
      remove(drop);
    }
    memcpy(items_[count_].text, t, sizeof(t));
    items_[count_].done = false;
    ++count_;
    return true;
  }
  bool setDone(int i, bool done) {
    if (i < 0 || i >= count_) return false;
    items_[i].done = done;
    return true;
  }
  bool remove(int i) {
    if (i < 0 || i >= count_) return false;
    for (int k = i; k + 1 < count_; ++k) items_[k] = items_[k + 1];
    --count_;
    return true;
  }
  void clear() { count_ = 0; }
  int clearDone() {
    int w = 0;
    for (int i = 0; i < count_; ++i) if (!items_[i].done) items_[w++] = items_[i];
    const int gone = count_ - w;
    count_ = w;
    return gone;
  }

  // Trims spaces and control characters at the ends, replaces any inside with
  // spaces, and cuts to kTextMax bytes without splitting a character.
  static bool clean(const char* in, char* out) {
    if (!in) in = "";
    while (*in && static_cast<uint8_t>(*in) <= ' ') ++in;
    size_t n = strlen(in);
    while (n && static_cast<uint8_t>(in[n - 1]) <= ' ') --n;
    if (n > static_cast<size_t>(kTextMax)) {
      n = kTextMax;
      while (n && (static_cast<uint8_t>(in[n]) & 0xC0) == 0x80) --n;   // not mid-character
    }
    for (size_t i = 0; i < n; ++i) out[i] = static_cast<uint8_t>(in[i]) < ' ' ? ' ' : in[i];
    out[n] = 0;
    return n > 0;
  }

 private:
  Item items_[kMax];
  int count_ = 0;
};

}  // namespace reminders
