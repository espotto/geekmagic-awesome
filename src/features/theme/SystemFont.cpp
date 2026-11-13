#include "SystemFont.h"
#include <cstring>
#ifdef ARDUINO
#include <pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(p) (*reinterpret_cast<const unsigned char*>(p))
#define memcpy_P memcpy
#endif
#include "SystemFontData.h"

namespace gm {
namespace {

const char* const kNames[TextStyleCount] = {"caption", "body", "headline", "display-s", "display-m", "display-l"};

const sysfontdata::Style& data(TextStyle style) {
  const uint8_t i = static_cast<uint8_t>(style);
  return sysfontdata::kStyles[i < TextStyleCount ? i : 0];
}

// Binary search over the style's ascending codes.
bool find(const sysfontdata::Style& s, uint8_t c, sysfontdata::Glyph& out) {
  int lo = 0, hi = s.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint8_t code = pgm_read_byte(s.codes + mid);
    if (code == c) {
      memcpy_P(&out, s.glyphs + mid, sizeof(out));
      return true;
    }
    if (code < c) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

void view(const sysfontdata::Style& s, const sysfontdata::Glyph& g, int shift, GlyphView& out) {
  out.bitmap = s.bitmap + g.offset;
  out.advance = g.advance;
  out.width = g.width;
  out.height = g.height;
  out.left = g.left;
  out.top = static_cast<int16_t>(g.top + shift);
}

}  // namespace

// Decodes the UTF-8 character at s[i] into its one-byte display code (see
// displayText in SystemFont.h) and advances `i` past it. False, with `i`
// past the offending bytes, for a code point the font cannot draw or for
// malformed UTF-8; control characters and DEL are refused too.
bool decodeDisplayChar(const char* s, size_t size, size_t& i, uint8_t& out) {
  const unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    ++i;
    out = c;
    return c >= 32 && c <= 126;
  }
  if ((c & 0xe0) != 0xc0 || i + 1 >= size || (static_cast<unsigned char>(s[i + 1]) & 0xc0) != 0x80) {
    ++i;
    while (i < size && (static_cast<unsigned char>(s[i]) & 0xc0) == 0x80) ++i;
    return false;
  }
  const unsigned code = ((c & 0x1fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3fu);
  i += 2;
  if (code >= 0x410 && code <= 0x44f) out = static_cast<uint8_t>(0xc0 + (code - 0x410));
  else if (code == 0x401) out = 0xa8;  // Ё
  else if (code == 0x451) out = 0xb8;  // ё
  else if (code == 0xb0) out = 0xb0;   // degree sign
  else return false;
  return true;
}
std::string displayText(const std::string& utf8) {
  std::string out;
  out.reserve(utf8.size());
  for (size_t i = 0; i < utf8.size();) {
    uint8_t c = 0;
    out += decodeDisplayChar(utf8.data(), utf8.size(), i, c) ? static_cast<char>(c) : '?';
  }
  return out;
}
size_t displayTextTo(char* out, size_t capacity, const char* utf8) {
  if (!out || !capacity) return 0;
  size_t n = 0;
  const size_t size = utf8 ? strlen(utf8) : 0;
  for (size_t i = 0; i < size && n + 1 < capacity;) {
    const unsigned char c = static_cast<unsigned char>(utf8[i]);
    if (c < 32 || c == 127) {  // control characters are dropped, not drawn as '?'
      ++i;
      continue;
    }
    uint8_t code = 0;
    out[n++] = decodeDisplayChar(utf8, size, i, code) ? static_cast<char>(code) : '?';
  }
  out[n] = 0;
  return n;
}

bool textStyleFromName(const char* name, size_t length, TextStyle& out) {
  for (uint8_t i = 0; i < TextStyleCount; ++i)
    if (strlen(kNames[i]) == length && !memcmp(kNames[i], name, length)) {
      out = static_cast<TextStyle>(i);
      return true;
    }
  return false;
}
const char* textStyleName(TextStyle style) { return kNames[static_cast<uint8_t>(style) < TextStyleCount ? static_cast<uint8_t>(style) : 0]; }
TextStyle textStyleForSize(int size) {
  return size <= 18 ? TextStyle::Caption : size <= 24 ? TextStyle::Body : size <= 38 ? TextStyle::Headline
       : size <= 58 ? TextStyle::DisplayS : size <= 82 ? TextStyle::DisplayM : TextStyle::DisplayL;
}
int styleSize(TextStyle style) { return data(style).size; }
int styleBaseline(TextStyle style) { return data(style).baseline; }
int styleLineHeight(TextStyle style) { return data(style).lineHeight; }

void styleGlyph(TextStyle style, uint8_t c, GlyphView& out) {
  const sysfontdata::Style& s = data(style);
  sysfontdata::Glyph g;
  if (find(s, c, g)) return view(s, g, 0, out);
  const sysfontdata::Style& h = data(TextStyle::Headline);
  if (style > TextStyle::Headline && find(h, c, g)) return view(h, g, s.baseline - h.baseline, out);
  out = GlyphView();
  out.advance = find(s, ' ', g) ? g.advance : static_cast<uint8_t>(s.size / 4);
}

uint8_t glyphCoverage(const GlyphView& g, int x, int y) {
  if (x < 0 || y < 0 || x >= g.width || y >= g.height) return 0;
  const int k = y * g.width + x;
  return static_cast<uint8_t>(((pgm_read_byte(g.bitmap + k / 4) >> (6 - 2 * (k & 3))) & 3) * 85);
}

size_t writeSystemFont(FontSink emit, void* context) {
  size_t total = 0;
  uint8_t buf[64];
  auto put = [&](const uint8_t* p, size_t n) {
    if (emit) emit(context, p, n);
    total += n;
  };
  // Copies `n` bytes out of flash through `buf`.
  auto putFlash = [&](const uint8_t* p, size_t n) {
    while (n) {
      const size_t k = n < sizeof(buf) ? n : sizeof(buf);
      memcpy_P(buf, p, k);
      put(buf, k);
      p += k;
      n -= k;
    }
  };
  const uint8_t head[5] = {'G', 'M', 'F', '1', TextStyleCount};
  put(head, sizeof(head));
  for (uint8_t i = 0; i < TextStyleCount; ++i) {
    const sysfontdata::Style& s = sysfontdata::kStyles[i];
    const uint8_t info[6] = {s.count, s.size, s.baseline, s.lineHeight,
                             static_cast<uint8_t>(s.bitmapSize & 0xff), static_cast<uint8_t>(s.bitmapSize >> 8)};
    put(info, sizeof(info));
    putFlash(s.codes, s.count);
    for (uint8_t g = 0; g < s.count; ++g) {
      sysfontdata::Glyph glyph;
      memcpy_P(&glyph, s.glyphs + g, sizeof(glyph));
      const uint8_t row[8] = {static_cast<uint8_t>(glyph.offset & 0xff), static_cast<uint8_t>(glyph.offset >> 8),
                              glyph.advance, glyph.width, glyph.height,
                              static_cast<uint8_t>(glyph.left), static_cast<uint8_t>(glyph.top), 0};
      put(row, sizeof(row));
    }
    putFlash(s.bitmap, s.bitmapSize);
  }
  return total;
}

int styleTextWidth(TextStyle style, const char* text, size_t length) {
  int width = 0;
  GlyphView g;
  for (size_t i = 0; i < length; ++i) {
    styleGlyph(style, static_cast<uint8_t>(text[i]), g);
    width += g.advance;
  }
  return width;
}

}  // namespace gm
