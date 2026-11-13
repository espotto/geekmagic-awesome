// Localized text: a text `value` per language under spec 2,
// the fallback to English, the fixed limits, and month/weekday names that
// follow the device language -- for spec 2 themes only.
#include "ThemeEngine.h"
#include <cassert>
#include <iostream>
using namespace gm;

static std::string manifest(int spec, const std::string& value) {
  return "{\"spec\":" + std::to_string(spec) + ",\"theme\":{\"id\":\"t\",\"name\":\"T\",\"author\":\"A\",\"version\":\"1\"},"
    "\"display\":{\"width\":240,\"height\":240,\"background\":\"#000000\"},\"layers\":[{\"id\":\"l\",\"type\":\"text\","
    "\"x\":0,\"y\":0,\"value\":" + value + ",\"size\":16,\"color\":\"#ffffff\"}]}";
}
static bool parses(int spec, const std::string& value, Theme& t, std::string& error) {
  return parseTheme(manifest(spec, value), t, error);
}
static bool parses(int spec, const std::string& value) {
  Theme t;
  std::string error;
  return parses(spec, value, t, error);
}
static std::string rendered(const Theme& theme, uint8_t language, const tm& t) {
  Engine engine;
  engine.setTheme(theme, 0);
  engine.setLanguage(language);
  engine.update(0, &t);
  return engine.states()[0].text;
}

int main() {
  Theme theme;
  std::string error;
  tm t{};
  t.tm_year = 126; t.tm_mon = 8; t.tm_mday = 17; t.tm_wday = 4;  // Thursday 17 September 2026

  // Parse and fall back to English.
  // pt, es: languages the device once had; the theme still installs and they are left out.
  assert(parses(2, R"({"en":"Hello {DD}","ru":"Privet {DD}","pt":"Ola {DD}","es":"Hola {DD}"})", theme, error));
  assert(theme.spec == 2 && theme.layers[0].value == "Hello {DD}" && theme.translations.size() == 1);
  assert(rendered(theme, 0, t) == "Hello 17");
  assert(rendered(theme, languageFromCode("ru"), t) == "Privet 17");
  assert(rendered(theme, languageFromCode("pt"), t) == "Hello 17");  // no longer a language: English
  assert(parses(2, R"({"en":"Hello {DD}"})", theme, error));
  assert(rendered(theme, languageFromCode("ru"), t) == "Hello 17");  // not given: English
  assert(rendered(theme, languageFromCode("xx"), t) == "Hello 17");  // not a language: English
  assert(rendered(theme, 200, t) == "Hello 17");
  // A plain string still works under spec 2, in every language.
  assert(parses(2, "\"{WEEKDAY}\"", theme, error) && theme.translations.empty());
  assert(rendered(theme, 0, t) == "Thursday");

  // Two localized layers keep their own texts.
  {
    const std::string json = manifest(2, R"({"en":"A","ru":"B","it":"C"})");
    std::string two = json;
    two.replace(two.size() - 2, 2, R"(,{"id":"m","type":"text","x":0,"y":20,"value":{"en":"D","ru":"E","es":"F"},"size":16,"color":"#ffffff"},)"
      R"({"id":"n","type":"text","x":0,"y":40,"value":"G","size":16,"color":"#ffffff"}]})");
    assert(parseTheme(two, theme, error));
    Engine engine;
    engine.setTheme(theme, 0);
    engine.setLanguage(languageFromCode("ru"));
    engine.update(0, &t);
    assert(engine.states()[0].text == "B" && engine.states()[1].text == "E" && engine.states()[2].text == "G");
    engine.setLanguage(0);
    engine.update(5, &t);
    assert(engine.states()[0].text == "A" && engine.states()[1].text == "D" && engine.states()[2].text == "G");
  }

  // Changing the language re-expands the text on the next update.
  {
    assert(parses(2, R"({"en":"One","ru":"Um"})", theme, error));
    Engine engine;
    engine.setTheme(theme, 0);
    engine.update(0, &t);
    assert(engine.states()[0].text == "One");
    engine.setLanguage(languageFromCode("ru"));
    engine.update(10, &t);
    assert(engine.states()[0].text == "Um");
    engine.setLanguage(0);
    engine.update(20, &t);
    assert(engine.states()[0].text == "One");
  }

  // Spec 1 refuses an object, and ignores the device language.
  assert(!parses(1, R"({"en":"Hello"})", theme, error) && error.find("spec") != std::string::npos);
  assert(parses(1, "\"{MON} {WD}\"", theme, error));
  assert(rendered(theme, languageFromCode("ru"), t) == "Sep Thu");
  // A spec 1 theme is untouched by the translations member.
  assert(parses(1, "\"x\"", theme, error) && theme.translations.empty());

  // Validation of the object.
  assert(!parses(2, R"({})"));
  assert(!parses(2, R"({"ru":"Ola"})"));                // no English to fall back to
  assert(!parses(2, R"({"pt":"Ola"})"));
  assert(parses(2, R"({"en":"a","pt":"","de":5})"));    // a former language is not looked at
  assert(!parses(2, R"({"en":"a","zz":"b"})"));         // unknown language
  assert(!parses(2, R"({"en":"a","RU":"b"})"));         // codes are lower case
  assert(!parses(2, R"({"en":"a","ru":""})"));          // empty
  assert(!parses(2, R"({"en":"a","ru":5})"));           // not a string
  assert(!parses(2, R"({"en":"a","ru":{"x":"y"}})"));   // nested
  assert(!parses(2, R"({"en":"a","ru":["b"]})"));
  assert(!parses(2, R"({"en":"a","ru":"{nope}"})"));    // undeclared variable
  assert(!parses(2, R"({"en":"a","ru":"unmatched }"})"));
  assert(!parses(2, R"({"en":"a","ru":"café"})")); // not ASCII
  assert(!parses(2, R"({"en":"a","ru":"b\u0001"})"));
  assert(!parses(2, R"([])"));
  assert(!parses(2, R"(null)"));
  // Limits: 128 bytes per language.
  const std::string max(128, 'x'), over(129, 'x');
  assert(parses(2, "{\"en\":\"" + max + "\",\"ru\":\"" + max + "\"}"));
  assert(!parses(2, "{\"en\":\"a\",\"ru\":\"" + over + "\"}"));
  assert(!parses(2, "{\"en\":\"" + over + "\"}"));
  // Data variables are checked in every language.
  {
    const std::string json =
      R"({"spec":2,"theme":{"id":"t","name":"T","author":"A","version":"1"},"display":{"width":240,"height":240,"background":"#000000"},)"
      R"("data":[{"id":"w","url":"https://example.test/w.json","interval":60,"insecureTls":true,"fields":[{"id":"temp","path":"t"}]}],)"
      R"("layers":[{"id":"l","type":"text","x":0,"y":0,"value":{"en":"{w.temp} C","ru":"{w.temp} graus"},"size":16,"color":"#ffffff"}]})";
    assert(parseTheme(json, theme, error));
    Engine engine;
    engine.setTheme(theme, 0);
    engine.setValues({{"w.temp", "21"}});
    engine.setLanguage(languageFromCode("ru"));
    engine.update(0, &t);
    assert(engine.states()[0].text == "21 graus");
    std::string bad = json;
    bad.replace(bad.find("{w.temp} graus"), 14, "{w.gone} graus");
    assert(!parseTheme(bad, theme, error));
  }

  // Month and weekday names by language, spec 2; abbreviations are 3 letters.
  struct Row { const char* code; const char* text; } rows[] = {
    {"en", "Thu Thursday Sep September"},
    {"ru", "чт Четверг сен сентября"},
  };
  for (const Row& row : rows)
    assert(expandText("{WD} {WEEKDAY} {MON} {MONTH}", &t, {}, languageFromCode(row.code)) == row.text);
  // Every name of every language: 2..13 letters the font draws, no overrun of
  // its slot. Lengths count display characters (one byte each after displayText).
  for (uint8_t language = 0; language < kLanguageCount; ++language) {
    for (int month = 0; month < 12; ++month) {
      tm m{}; m.tm_mon = month; m.tm_wday = month % 7;
      for (const char* token : {"{MON}", "{MONTH}", "{WD}", "{WEEKDAY}"}) {
        std::string name = displayText(expandText(token, &m, {}, language));
        assert(name.size() >= (language == languageFromCode("ru") ? 2u : 3u) && name.size() <= 13);
        if (std::string(token) == "{MON}" || std::string(token) == "{WD}") assert(name.size() <= 4);   // {MON} {WD}: an abbreviation
        for (char c : name) assert((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || static_cast<unsigned char>(c) >= 0xc0);
      }
    }
  }
  for (int wday = 0; wday < 7; ++wday) {   // the longest names fit whole
    tm m{}; m.tm_wday = wday; m.tm_mon = 8;
    assert(expandText("{WEEKDAY}", &m, {}, 1).size() >= 6);
  }
  {
    // No abbreviation is shared by two months in any language, nor by two days.
    for (uint8_t language = 0; language < kLanguageCount; ++language) {
      for (int a = 0; a < 12; ++a) for (int b = a + 1; b < 12; ++b) {
        tm x{}, y{}; x.tm_mon = a; y.tm_mon = b;
        assert(expandText("{MON}", &x, {}, language) != expandText("{MON}", &y, {}, language));
      }
      for (int a = 0; a < 7; ++a) for (int b = a + 1; b < 7; ++b) {
        tm x{}, y{}; x.tm_wday = a; y.tm_wday = b;
        assert(expandText("{WD}", &x, {}, language) != expandText("{WD}", &y, {}, language));
      }
    }
  }
  // The stored text is returned as is, not copied: a translation or the English value.
  {
    assert(parses(2, R"({"en":"One","ru":"Um"})", theme, error));
    assert(&layerText(theme, theme.layers[0], languageFromCode("ru")) == &theme.translations[0].text);
    assert(&layerText(theme, theme.layers[0], 0) == &theme.layers[0].value);
    assert(&layerText(theme, theme.layers[0], languageFromCode("de")) == &theme.layers[0].value);
  }
  // The translations of a theme share 1024 bytes of RAM, each costing its entry
  // and, past the inline buffer, its text: as many of 128 characters as fit
  // and then short ones fit, one more does not.
  {
    const size_t full = sizeof(LocalizedText) + 129, brief = sizeof(LocalizedText);
    const size_t big = MaxTranslationBytes / full, small = (MaxTranslationBytes - big * full) / brief;
    auto manifest = [](size_t big, size_t small) {
      std::string json = "{\"spec\":2,\"theme\":{\"id\":\"t\",\"name\":\"T\",\"author\":\"A\",\"version\":\"1\"},"
        "\"display\":{\"width\":240,\"height\":240,\"background\":\"#000000\"},\"layers\":[";
      for (size_t i = 0; i < big + small; ++i)
        json += std::string(i ? "," : "") + "{\"id\":\"a" + std::to_string(i) + "\",\"type\":\"text\",\"x\":0,\"y\":0,\"size\":16,\"color\":\"#ffffff\",\"value\":{\"en\":\"a\","
        "\"ru\":\"" + std::string(i < big ? 128 : 1, 'x') + "\"}}";
      return json + "]}";
    };
    assert(parseTheme(manifest(big, small), theme, error));
    assert(theme.translations.size() == big + small && theme.translationBytes == big * full + small * brief);
    assert(!parseTheme(manifest(big, small + 1), theme, error) && error.find("1024") != std::string::npos);
  }
  assert(expandText("{WEEKDAY}", &t, {}, 99) == "Thursday");     // out of range: English
  assert(expandText("{WEEKDAY}", &t) == "Thursday");             // the old overload: English
  assert(expandText("{HH}:{MM}", nullptr, {}, 1) == "--:--");
  assert(languageFromCode(nullptr) == 0 && languageFromCode("") == 0 && languageFromCode("pt-BR") == 0);
  assert(kLanguageCount == 2 && std::string(languageCode(0)) == "en" && std::string(languageCode(9)) == "en");
  assert(std::string(languageCode(1)) == "ru" && languageFromCode("ru") == 1);
  for (const char* former : {"pt", "es", "fr", "de", "it"}) assert(languageFromCode(former) == 0);   // English now

  // Cyrillic: accepted in templates and translations, drawn one byte per letter.
  {
    assert(parses(1, "\"Привет {DD}\"", theme, error));
    Engine engine;
    engine.setTheme(theme, 0);
    engine.update(0, &t);
    assert(engine.states()[0].text == displayText("Привет 17") && engine.states()[0].text.size() == 9);
    assert(parses(2, R"({"en":"Hi","ru":"Ёлка ё 5°"})", theme, error));
    assert(rendered(theme, languageFromCode("ru"), t) == "\xA8\xEB\xEA\xE0 \xB8 5\xB0");
    assert(rendered(theme, languageFromCode("de"), t) == "Hi");
    assert(parses(2, "\"{WD} {DD} {MON}\"", theme, error));
    assert(rendered(theme, languageFromCode("ru"), t) == displayText("чт 17 сен"));
  }
  assert(displayText("АЯаяЁё°") == "\xC0\xDF\xE0\xFF\xA8\xB8\xB0");
  assert(displayText("abc -5") == "abc -5");
  {  // displayTextTo: fixed buffer, control characters dropped, truncation by letters
    char buf[8];
    assert(displayTextTo(buf, sizeof(buf), "Привет\n мир") == 7 && std::string(buf) == displayText("Привет "));
    assert(displayTextTo(buf, sizeof(buf), "A\tБ😀") == 3 && std::string(buf) == "A\xC1?");
    assert(displayTextTo(buf, sizeof(buf), nullptr) == 0 && buf[0] == 0);
    assert(displayTextTo(buf, 1, "abc") == 0 && buf[0] == 0);
    assert(displayTextTo(nullptr, 0, "abc") == 0);
  }
  assert(displayText("Ωé\xC3x\xFF\x01") == "???x??");          // other scripts, broken UTF-8, control bytes
  assert(!parses(1, "\"Ωmega\""));                               // a letter the font lacks
  assert(!parses(1, "\"bad \\u0001\""));

  std::cout << "language tests passed\n";
}
