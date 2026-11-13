#include "CurrencyView.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../AppDraw.h"

namespace currencyview {
namespace {

using gm::TextStyle;
using appdraw::mix;

// code|English|Russian, in the order the web page lists them.
const char kNames[] PROGMEM =
  "USD|US Dollar|Доллар США|"
  "EUR|Euro|Евро|"
  "CNY|Chinese Yuan|Юань|"
  "GBP|British Pound|Фунт стерлингов|"
  "JPY|Japanese Yen|Иена|"
  "CHF|Swiss Franc|Швейцарский франк|"
  "TRY|Turkish Lira|Турецкая лира|"
  "AED|UAE Dirham|Дирхам ОАЭ|"
  "KZT|Kazakh Tenge|Тенге|"
  "BYN|Belarusian Ruble|Белорусский рубль|"
  "UAH|Ukrainian Hryvnia|Гривна|"
  "GEL|Georgian Lari|Лари|"
  "AMD|Armenian Dram|Драм|"
  "UZS|Uzbek Sum|Сум|"
  "THB|Thai Baht|Бат|"
  "INR|Indian Rupee|Индийская рупия|"
  "HKD|Hong Kong Dollar|Гонконгский доллар|"
  "CAD|Canadian Dollar|Канадский доллар|"
  "AUD|Australian Dollar|Австралийский доллар|"
  "PLN|Polish Zloty|Злотый";
constexpr int kKnown = 20;

// Per language: the title, the month names, "loading" and "failed".
const char kWords[] PROGMEM =
  "Rates|Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec|Loading rates...|No rates: no connection|"
  "Курсы ЦБ|янв.|февр.|марта|апр.|мая|июня|июля|авг.|сент.|окт.|нояб.|дек.|Загрузка курсов...|Нет курсов: нет связи";
constexpr int kPerLang = 15;

const uint16_t kText = 0xFFFF;
const uint16_t kGrey = 0x8C72;      // #8e8e93
const uint16_t kRule = 0x2945;      // #2c2c2e
const uint16_t kGreen = 0x368B;     // #30d158
const uint16_t kRed = 0xFA27;       // #ff453a
const uint16_t kFlat = 0x4A69;      // #48484a

constexpr int kLeft = 22, kRight = 218;
constexpr int kTitleTop = 17, kRuleY = 48;
constexpr int kRowTop = 54, kRowH = 42;
constexpr int kPillH = 19, kPillDy = 21;   // the pill's top below the row's

int pillWidth(const Row& r) {
  const int w = appdraw::width(TextStyle::Caption, r.change) + 12;
  return w < 62 ? 62 : w;
}

// The `index`th entry of kNames.
void nameEntry(int index, char* out, size_t cap) { appdraw::entry(kNames, index, out, cap); }

}  // namespace

int knownCount() { return kKnown; }

const char* knownCode(int i) {
  static char code[4];
  nameEntry(i * 3, code, sizeof(code));
  return code;
}

bool isKnown(const char* code) {
  for (int i = 0; i < kKnown; ++i)
    if (!strcmp(knownCode(i), code)) return true;
  return false;
}

void currencyName(const char* code, uint8_t lang, char* out, size_t cap) {
  if (lang > 1) lang = 0;
  for (int i = 0; i < kKnown; ++i)
    if (!strcmp(knownCode(i), code)) { nameEntry(i * 3 + 1 + lang, out, cap); return; }
  snprintf(out, cap, "%s", code);
}

int parseCodes(const char* list, char codes[kMax][4]) {
  int n = 0;
  const char* p = list ? list : "";
  while (*p && n < kMax) {
    while (*p == ',' || *p == ' ') ++p;
    int k = 0;
    char c[4] = {0};
    while (*p && *p != ',' && *p != ' ') {
      if (k < 3) c[k] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
      ++k;
      ++p;
    }
    bool ok = k == 3;
    for (int i = 0; i < 3 && ok; ++i) ok = c[i] >= 'A' && c[i] <= 'Z';
    for (int i = 0; i < n && ok; ++i) ok = strcmp(codes[i], c) != 0;   // once each
    if (ok) memcpy(codes[n++], c, 4);
  }
  return n;
}

void formatValue(float v, bool comma, char* out, size_t cap) {
  snprintf(out, cap, v < 1 ? "%.4f" : "%.2f", (double)v);
  if (comma)
    for (char* c = out; *c; ++c)
      if (*c == '.') *c = ',';
}

void formatChange(float value, float prev, bool comma, char* out, size_t cap, int8_t& trend) {
  const float pct = prev > 0 ? (value - prev) / prev * 100.0f : 0.0f;
  const float shown = roundf(pct * 100.0f) / 100.0f;   // what two decimals will say
  trend = shown > 0 ? 1 : shown < 0 ? -1 : 0;
  snprintf(out, cap, "%s%.2f%%", trend > 0 ? "+" : trend < 0 ? "-" : "", (double)fabsf(shown));
  if (comma)
    for (char* c = out; *c; ++c)
      if (*c == '.') *c = ',';
}

void layout(const Data& d, Empty empty, uint8_t lang, Layout& out) {
  out = Layout();
  if (lang > 1) lang = 0;
  const bool ru = lang == 1;
  appdraw::word(kWords, lang * kPerLang, out.title, sizeof(out.title));
  out.valid = d.valid;
  const int dateRoom = 60;   // "14 нояб." and a gap
  appdraw::cutToWidth(TextStyle::Body, out.title, sizeof(out.title), kRight - kLeft - dateRoom);
  if (!d.valid) {
    appdraw::word(kWords, lang * kPerLang + (empty == Empty::Failed ? 14 : 13), out.message, sizeof(out.message));
    return;
  }
  if (d.day && d.month >= 1 && d.month <= 12) {
    char m[16], utf8[24];
    appdraw::entry(kWords, lang * kPerLang + d.month, m, sizeof(m));
    if (ru) snprintf(utf8, sizeof(utf8), "%u %s", (unsigned)d.day, m);
    else snprintf(utf8, sizeof(utf8), "%s %u", m, (unsigned)d.day);
    gm::displayTextTo(out.date, sizeof(out.date), utf8);
  }
  out.count = d.count > kMax ? kMax : d.count;
  for (int i = 0; i < out.count; ++i) {
    const Rate& r = d.rates[i];
    Row& w = out.rows[i];
    memcpy(w.code, r.code, 4);
    char name[48];
    currencyName(r.code, lang, name, sizeof(name));
    gm::displayTextTo(w.name, sizeof(w.name), name);
    if (r.valid) {
      formatValue(r.value, ru, w.value, sizeof(w.value));
      formatChange(r.value, r.prev, ru, w.change, sizeof(w.change), w.trend);
    } else {
      strcpy(w.value, "--");
    }
    // The name keeps clear of the pill beside it.
    const int room = kRight - (w.change[0] ? pillWidth(w) : 0) - 10 - kLeft;
    appdraw::cutToWidth(TextStyle::Caption, w.name, sizeof(w.name), room);
  }
}

void renderRow(const Layout& l, int y, uint16_t* row) {
  for (int x = 0; x < appdraw::kWidth; ++x) row[x] = 0;
  appdraw::text(TextStyle::Body, l.title, kLeft, kTitleTop, y, kText, row);
  if (l.date[0]) {
    const int w = appdraw::width(TextStyle::Caption, l.date);
    appdraw::text(TextStyle::Caption, l.date, kRight - w, kTitleTop + 4, y, kGrey, row);
  }
  if (y == kRuleY)
    for (int x = kLeft; x < kRight; ++x) row[x] = kRule;
  if (!l.valid) {
    appdraw::textCentered(TextStyle::Caption, l.message, 120, 112, y, kGrey, row);
    return;
  }
  for (int i = 0; i < l.count; ++i) {
    const Row& r = l.rows[i];
    const int top = kRowTop + i * kRowH;
    if (y < top - 4 || y > top + kRowH) continue;
    appdraw::text(TextStyle::Body, r.code, kLeft, top - 2, y, kText, row);
    appdraw::text(TextStyle::Caption, r.name, kLeft, top + kPillDy - 2, y, kGrey, row);
    const int vw = appdraw::width(TextStyle::Body, r.value);
    appdraw::text(TextStyle::Body, r.value, kRight - vw, top - 2, y, kText, row);
    if (r.change[0]) {
      const int pw = pillWidth(r), px = kRight - pw, py = top + kPillDy;
      const uint16_t c = r.trend > 0 ? kGreen : r.trend < 0 ? kRed : kFlat;
      for (int x = px; x < kRight; ++x) {
        const int a = appdraw::roundRect(x, y, px, py, kRight, py + kPillH, 5);
        if (a) row[x] = mix(row[x], c, a);
      }
      const int cw = appdraw::width(TextStyle::Caption, r.change);
      appdraw::text(TextStyle::Caption, r.change, px + (pw - cw) / 2, py - 1, y, kText, row);
    }
  }
}

}  // namespace currencyview
