#include "text/glyph_class.h"

#include <raylib.h>

#include <cstddef>

namespace {

struct CodepointRange {
  int first;
  int last;
};

constexpr CodepointRange kBaseRanges[] = {
    {32, 126},          // printable ASCII
    {160, 255},         // Latin-1 Supplement
    {256, 383},         // Latin Extended-A
    {0x2010, 0x2027},   // dashes, curly quotes, ellipsis (includes 0x2022 BULLET)
    {0x25CB, 0x25CB},   // WHITE CIRCLE — 2nd-level unordered list marker
    {0x25AA, 0x25AA},   // BLACK SMALL SQUARE — 3rd-level unordered list marker
};

// Unicode ranges with a meaningful emoji presence (symbols, dingbats, ZWJ
// and the emoji presentation selector so combined sequences don't break,
// plus the entire supplementary plane where the "modern" emoji blocks
// live — deliberately generous: used only for classification, not to
// assemble the emoji font atlas, which only loads the codepoints actually
// used in the document, see
// render/text_renderer.cpp::ensure_extra_fonts_loaded).
constexpr CodepointRange kEmojiRanges[] = {
    {0x203C, 0x203C}, {0x2049, 0x2049}, {0x2122, 0x2122}, {0x2139, 0x2139},
    {0x2194, 0x21AA}, {0x231A, 0x231B}, {0x2328, 0x2328}, {0x23CF, 0x23CF},
    {0x23E9, 0x23FA}, {0x24C2, 0x24C2}, {0x25FB, 0x25FE}, {0x2600, 0x27BF},
    {0x2934, 0x2935}, {0x2B00, 0x2BFF}, {0x200D, 0x200D}, {0xFE0F, 0xFE0F},
    {0x1F000, 0x1FFFF},
};

// Unicode ranges for Asian (CJK) scripts: punctuation, Hiragana/Katakana,
// Bopomofo, Jamo/Hangul compatibility, CJK compatibility symbols/forms,
// unified ideographs (BMP and supplementary planes), Hangul, and
// halfwidth/fullwidth forms.
constexpr CodepointRange kAsianRanges[] = {
    {0x3000, 0x303F}, {0x3040, 0x30FF}, {0x3100, 0x312F}, {0x3130, 0x318F},
    {0x3190, 0x31FF}, {0x3200, 0x33FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF},
    {0xA000, 0xA4CF}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF}, {0xFF00, 0xFFEF},
    {0x20000, 0x2A6DF}, {0x2A700, 0x2EBEF},
};

template <std::size_t N>
bool in_ranges(const CodepointRange (&ranges)[N], int codepoint) {
  for (const CodepointRange& range : ranges) {
    if (codepoint >= range.first && codepoint <= range.last) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<int> base_codepoints() {
  std::vector<int> codepoints;
  for (const CodepointRange& range : kBaseRanges) {
    for (int c = range.first; c <= range.last; ++c) {
      codepoints.push_back(c);
    }
  }
  return codepoints;
}

GlyphFontKind classify_codepoint(int codepoint) {
  if (in_ranges(kBaseRanges, codepoint)) {
    return GlyphFontKind::Base;
  }
  if (in_ranges(kEmojiRanges, codepoint)) {
    return GlyphFontKind::Emoji;
  }
  if (in_ranges(kAsianRanges, codepoint)) {
    return GlyphFontKind::Asian;
  }
  return GlyphFontKind::Other;
}

std::vector<GlyphRun> split_by_glyph_kind(const std::string& text) {
  std::vector<GlyphRun> runs;

  size_t i = 0;
  while (i < text.size()) {
    int codepoint_size = 0;
    int codepoint = GetCodepointNext(text.c_str() + i, &codepoint_size);
    if (codepoint_size <= 0) {
      codepoint_size = 1;  // should never happen (GetCodepointNext always advances at least 1), defensive
    }
    GlyphFontKind kind = classify_codepoint(codepoint);

    if (!runs.empty() && runs.back().kind == kind) {
      runs.back().text.append(text, i, static_cast<size_t>(codepoint_size));
    } else {
      runs.push_back(GlyphRun{text.substr(i, static_cast<size_t>(codepoint_size)), kind});
    }

    i += static_cast<size_t>(codepoint_size);
  }

  return runs;
}
