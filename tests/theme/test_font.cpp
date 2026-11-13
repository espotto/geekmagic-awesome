// The system font (SystemFont.h) and how themes use it: style names, the
// size-to-style rule for older themes, glyph lookup and fallback, text width,
// layer bounds and rendering.
#include "SystemFont.h"
#include "ThemeEngine.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace gm;

static std::string manifest(const std::string& layer, const std::string& extra = "") {
  return "{\"spec\":2,\"theme\":{\"id\":\"t\",\"name\":\"T\",\"author\":\"A\",\"version\":\"1\"},"
    "\"display\":{\"width\":240,\"height\":240,\"background\":\"#000000\"}," + extra + "\"layers\":[" + layer + "]}";
}
static bool parses(const std::string& json, Theme& t) {
  std::string error;
  return parseTheme(json, t, error);
}
static bool parses(const std::string& json) {
  Theme t;
  return parses(json, t);
}
struct Screen : Display {
  uint16_t data[240 * 240]{};
  void row(int x, int y, const uint16_t* p, int n) override {
    for (int i = 0; i < n; ++i) data[y * 240 + x + i] = p[i];
  }
};
struct NoAssets : Assets {
  AssetHandle resolve(const std::string&) override { return InvalidAsset; }
  bool row(AssetHandle, int, int, int, uint16_t*, uint8_t*) override { return false; }
};

int main() {
  // Style names, both ways, and nothing else.
  const char* names[] = {"caption", "body", "headline", "display-s", "display-m", "display-l"};
  for (uint8_t i = 0; i < TextStyleCount; ++i) {
    TextStyle s = TextStyle::Caption;
    assert(textStyleFromName(names[i], std::string(names[i]).size(), s) && static_cast<uint8_t>(s) == i);
    assert(std::string(textStyleName(s)) == names[i]);
    assert(styleLineHeight(s) > styleSize(s) && styleBaseline(s) < styleLineHeight(s));
  }
  TextStyle s = TextStyle::Caption;
  assert(!textStyleFromName("display", 7, s) && !textStyleFromName("Caption", 7, s) && !textStyleFromName("", 0, s));
  assert(styleSize(TextStyle::Caption) == 17 && styleSize(TextStyle::Headline) == 28 && styleSize(TextStyle::DisplayM) == 70);
  // A size alone picks the nearest style.
  assert(textStyleForSize(8) == TextStyle::Caption && textStyleForSize(16) == TextStyle::Caption);
  assert(textStyleForSize(24) == TextStyle::Body && textStyleForSize(28) == TextStyle::Headline);
  assert(textStyleForSize(40) == TextStyle::DisplayS && textStyleForSize(70) == TextStyle::DisplayM && textStyleForSize(96) == TextStyle::DisplayL);

  // Glyphs: ink inside its box, Cyrillic present, unknown bytes blank.
  GlyphView g;
  styleGlyph(TextStyle::Caption, 'A', g);
  assert(g.width > 0 && g.height > 0 && g.advance > 0);
  int ink = 0;
  for (int y = 0; y < g.height; ++y) for (int x = 0; x < g.width; ++x) ink += glyphCoverage(g, x, y) > 0;
  assert(ink > 0 && glyphCoverage(g, -1, 0) == 0 && glyphCoverage(g, g.width, 0) == 0);
  styleGlyph(TextStyle::Headline, 0xC0, g);   // А
  assert(g.width > 0);
  styleGlyph(TextStyle::Body, 0x80, g);       // no such character: a space's advance, no ink
  assert(g.width == 0 && g.advance > 0);
  // Display styles: digits of their own, letters from headline on their baseline.
  GlyphView digit, letter, headlineLetter;
  styleGlyph(TextStyle::DisplayM, '5', digit);
  styleGlyph(TextStyle::DisplayM, 'C', letter);
  styleGlyph(TextStyle::Headline, 'C', headlineLetter);
  assert(digit.height > 40 && letter.height == headlineLetter.height && letter.width == headlineLetter.width);
  assert(letter.top == headlineLetter.top + styleBaseline(TextStyle::DisplayM) - styleBaseline(TextStyle::Headline));
  assert(letter.top + letter.height <= styleBaseline(TextStyle::DisplayM) + 1);   // sits on the baseline
  // Width is the sum of advances.
  GlyphView a, b;
  styleGlyph(TextStyle::Body, 'a', a);
  styleGlyph(TextStyle::Body, 'b', b);
  assert(styleTextWidth(TextStyle::Body, "ab", 2) == a.advance + b.advance && styleTextWidth(TextStyle::Body, "", 0) == 0);
  const std::string time = "12:34";
  assert(styleTextWidth(TextStyle::DisplayL, time.data(), time.size()) > styleTextWidth(TextStyle::DisplayS, time.data(), time.size()));

  // Themes: "font" names a style; anything else, or a root "fonts", is refused.
  Theme t;
  const std::string layer = R"({"id":"l","type":"text","x":120,"y":100,"anchor":"center","value":"12:34","font":"display-m","color":"#ffffff"})";
  assert(parses(manifest(layer), t) && t.layers[0].font == static_cast<uint8_t>(TextStyle::DisplayM) + 1);
  assert(!parses(manifest(R"({"id":"l","type":"text","x":0,"y":0,"value":"1","font":"huge","color":"#ffffff"})")));
  assert(!parses(manifest(layer, R"("fonts":[{"id":"f","size":20,"chars":"0"}],)")));
  // A named style fixes the size: a size binding is refused, scrolling is fine.
  assert(!parses(manifest(R"({"id":"l","type":"text","x":0,"y":0,"value":"1","font":"body","color":"#ffffff","bind":{"size":{"source":"time.second","input":[0,59],"output":[8,40]}}})")));
  assert(parses(manifest(R"({"id":"l","type":"text","x":0,"y":0,"value":"1","font":"body","color":"#ffffff","scroll":{"width":40,"mode":"loop","speed":10}})")));
  // Without a style, size is required and picks one.
  assert(!parses(manifest(R"({"id":"l","type":"text","x":0,"y":0,"value":"1","color":"#ffffff"})")));
  assert(parses(manifest(R"({"id":"l","type":"text","x":0,"y":0,"value":"1","size":16,"color":"#ffffff"})"), t) && t.layers[0].font == 0);

  // Bounds: the style's text width and line height, anchored.
  assert(parses(manifest(layer), t));
  Engine engine;
  engine.setTheme(t, 0);
  auto dirty = engine.update(0, nullptr);
  const int w = styleTextWidth(TextStyle::DisplayM, time.data(), time.size()), h = styleLineHeight(TextStyle::DisplayM);
  const Rect r = engine.states()[0].bounds;
  assert(r.w == w && r.h == h && r.x == 120 - w / 2 && r.y == 100 - h / 2);

  // Rendering: ink in the layer colour, nothing outside the bounds.
  Screen screen;
  NoAssets assets;
  assert(render(engine, dirty, assets, screen));
  int lit = 0, outside = 0;
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 240; ++x) {
      const bool in = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
      if (screen.data[y * 240 + x]) (in ? lit : outside)++;
      if (screen.data[y * 240 + x] == 0xffff) assert(in);
    }
  assert(lit > 500 && outside == 0);

  std::cout << "system font tests passed\n";
}
