// CurrencyView.h — the Exchange Rates app's screen, in the manner of a phone's
// Stocks app: "Курсы ЦБ" and the date of the rates on top, then a row per
// currency: its code and name on the left, the rate in roubles on the right
// with the day's change under it on a green or red pill. Official rates of the
// Bank of Russia. Rendered a row at a time, without Arduino (tests/apps).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace currencyview {

constexpr int kMax = 4;   // rows on the screen

struct Rate {
  char code[4] = {0};      // "USD"
  float value = 0;         // roubles for one unit
  float prev = 0;          // the previous day's, for one unit
  bool valid = false;      // the source had this code
};

struct Data {
  bool valid = false;
  uint8_t count = 0;
  Rate rates[kMax];
  uint8_t day = 0, month = 0;   // the date the rates are for (1-31, 1-12)
};

// Without data: the first fetch has not come back, or it failed.
enum class Empty : uint8_t { Loading, Failed };

struct Row {
  char code[4] = {0};
  char name[28] = {0};     // display bytes, cut to the column
  char value[16] = {0};    // "82,15", "0,5139"
  char change[12] = {0};   // "-1,00%"
  int8_t trend = 0;        // -1 down, 0 flat, 1 up
};

struct Layout {
  bool valid = false;
  char title[24] = {0};
  char date[16] = {0};
  char message[48] = {0};  // instead of the rows, without data
  uint8_t count = 0;
  Row rows[kMax];
};

// The currencies the app offers, in the order the web page lists them.
int knownCount();
const char* knownCode(int i);   // flash-free copy in RAM: the codes are short
bool isKnown(const char* code);
// The currency's name in `lang` (gm::languageCode index), UTF-8; the code
// itself for one the app does not know.
void currencyName(const char* code, uint8_t lang, char* out, size_t cap);

// "USD,EUR,CNY" into up to kMax codes; returns how many.
int parseCodes(const char* list, char codes[kMax][4]);

// A rate as the screen writes it: two decimals, four under 1; `comma` for
// Russian.
void formatValue(float v, bool comma, char* out, size_t cap);
void formatChange(float value, float prev, bool comma, char* out, size_t cap, int8_t& trend);

void layout(const Data& d, Empty empty, uint8_t lang, Layout& out);
void renderRow(const Layout& l, int y, uint16_t* row);

}  // namespace currencyview
