// The Exchange Rates app: the Bank of Russia's daily JSON read for the codes
// asked (CurrencyJson), the list of codes from the settings, the numbers as
// the screen writes them, and the screen (CurrencyView) inside the safe area.
// With a path argument sample screens go to a PPM.
#include "currency/CurrencyJson.h"
#include "ppm.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace currencyview;

static std::vector<uint16_t> frame(const Layout& l) {
  std::vector<uint16_t> px(240 * 240);
  for (int y = 0; y < 240; ++y) renderRow(l, y, &px[y * 240]);
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 240; ++x)
      if (px[y * 240 + x]) assert(x >= 19 && x < 221 && y >= 19 && y < 221);
  return px;
}

static const char* kSample =
  "{\"Date\":\"2026-11-14T11:30:00+03:00\",\"PreviousDate\":\"2026-11-13T11:30:00+03:00\","
  "\"Valute\":{\"USD\":{\"ID\":\"R01235\",\"CharCode\":\"USD\",\"Nominal\":1,\"Name\":\"x\",\"Value\":82.1516,\"Previous\":82.9803},"
  "\"EUR\":{\"Nominal\":1,\"Value\":95.4021,\"Previous\":94.8800},"
  "\"JPY\":{\"Nominal\":100,\"Value\":51.3897,\"Previous\":51.9862},"
  "\"CNY\":{\"Nominal\":1,\"Value\":12.3149,\"Previous\":12.3149}}}";

int main(int argc, char** argv) {
  char codes[kMax][4];
  assert(parseCodes("USD,EUR,CNY", codes) == 3 && !strcmp(codes[2], "CNY"));
  assert(parseCodes(" usd , eur,USD,XX,ABCD,JPY,GBP,CHF", codes) == 4);   // case, spaces, twice, junk, at most 4
  assert(!strcmp(codes[0], "USD") && !strcmp(codes[1], "EUR") && !strcmp(codes[2], "JPY") && !strcmp(codes[3], "GBP"));
  assert(parseCodes("", codes) == 0 && parseCodes(nullptr, codes) == 0);

  char s[16];
  int8_t t;
  formatValue(82.1516f, true, s, sizeof(s));
  assert(!strcmp(s, "82,15"));
  formatValue(0.513897f, false, s, sizeof(s));
  assert(!strcmp(s, "0.5139"));
  formatChange(82.1516f, 82.9803f, true, s, sizeof(s), t);
  assert(!strcmp(s, "-1,00%") && t == -1);
  formatChange(95.4021f, 94.88f, false, s, sizeof(s), t);
  assert(!strcmp(s, "+0.55%") && t == 1);
  formatChange(10.0f, 10.0001f, false, s, sizeof(s), t);   // rounds to nothing: flat, no sign
  assert(!strcmp(s, "0.00%") && t == 0);

  assert(isKnown("USD") && isKnown("PLN") && !isKnown("XDR") && knownCount() == 20);
  char name[48];
  currencyName("CHF", 1, name, sizeof(name));
  assert(!strcmp(name, "Швейцарский франк"));
  currencyName("XDR", 0, name, sizeof(name));
  assert(!strcmp(name, "XDR"));

  JsonDocument doc;
  assert(!deserializeJson(doc, kSample));
  Data d;
  const int n = parseCodes("USD,EUR,JPY,GBP", codes);
  assert(fromJson(doc.as<JsonObjectConst>(), codes, n, d));
  assert(d.count == 4 && d.day == 14 && d.month == 11);
  assert(d.rates[2].valid && d.rates[2].value > 0.5138f && d.rates[2].value < 0.514f);   // per yen, not per 100
  assert(!d.rates[3].valid);   // GBP is not in the sample

  // Through the filter the device uses: only the codes asked come out.
  JsonDocument filter, small;
  buildFilter(filter, codes, n);
  assert(!deserializeJson(small, kSample, DeserializationOption::Filter(filter)));
  assert(small["Valute"]["CNY"].isNull() && !small["Valute"]["USD"]["Value"].isNull() && small["Valute"]["USD"]["Name"].isNull());

  std::vector<std::vector<uint16_t>> frames;
  Layout l;
  layout(d, Empty::Loading, 1, l);
  assert(l.count == 4 && !strcmp(l.rows[0].value, "82,15") && !strcmp(l.rows[3].value, "--"));
  frames.push_back(frame(l));
  const int n3 = parseCodes("CNY,EUR,USD", codes);
  assert(fromJson(doc.as<JsonObjectConst>(), codes, n3, d));
  layout(d, Empty::Loading, 0, l);
  assert(l.rows[0].trend == 0 && !strcmp(l.rows[0].change, "0.00%"));
  frames.push_back(frame(l));
  // Long names are cut to keep clear of the pill.
  JsonDocument big;
  assert(!deserializeJson(big, "{\"Date\":\"2026-11-14T11:30:00+03:00\",\"Valute\":{\"AUD\":{\"Nominal\":1,\"Value\":55.8959,\"Previous\":55.0},"
                               "\"HKD\":{\"Nominal\":1,\"Value\":10.55,\"Previous\":10.61},\"UZS\":{\"Nominal\":10000,\"Value\":68.2,\"Previous\":68.4},"
                               "\"BYN\":{\"Nominal\":1,\"Value\":27.38,\"Previous\":27.31}}}"));
  const int n4 = parseCodes("AUD,HKD,UZS,BYN", codes);
  assert(fromJson(big.as<JsonObjectConst>(), codes, n4, d));
  layout(d, Empty::Loading, 1, l);
  assert(strstr(l.rows[0].name, "...") && !strcmp(l.rows[2].value, "0,0068"));
  frames.push_back(frame(l));
  layout(Data(), Empty::Failed, 1, l);
  frames.push_back(frame(l));
  if (argc > 1) writeFrames(argv[1], frames);
  std::cout << "currency tests passed\n";
}
