#pragma once

#include <string>
#include <vector>

// Font category needed to draw a Unicode codepoint. Base is everything
// already covered by the codepoint set loaded in every "normal" font
// (regular/bold/italic/bold-italic/mono — see base_codepoints() below);
// Emoji and Asian need the optional dedicated fonts passed via
// --emoji-font/--asian-font (see cli/cli_args.h); Other is any script not
// covered by any of the three (e.g. Cyrillic, Greek, Arabic) — there's no
// parameter to supply a font for it, so it's always drawn with the regular
// font, which doesn't have that glyph (shows up as '?'), and triggers a
// warning (see render/text_renderer.h::ensure_extra_fonts_loaded).
enum class GlyphFontKind {
  Base,
  Emoji,
  Asian,
  Other,
};

GlyphFontKind classify_codepoint(int codepoint);

// List of codepoints covered by every loaded "normal" font: printable ASCII
// + Latin-1 Supplement + Latin Extended-A + common typographic punctuation
// (dashes, curly quotes, ellipsis, bullet) + the two non-ASCII unordered
// list markers (hollow circle, small square). Single source of truth both
// for render/text_renderer.cpp to assemble the codepoint set of every
// loaded font and for classify_codepoint to recognize what's already
// covered.
std::vector<int> base_codepoints();

// A span of `text` (UTF-8 bytes) whose codepoints all belong to the same
// GlyphFontKind — see split_by_glyph_kind.
struct GlyphRun {
  std::string text;
  GlyphFontKind kind;
};

// Decodes `text` (UTF-8) and partitions it into consecutive spans of the
// same GlyphFontKind, in original order — used to route each piece of a
// word to the right font (see text/text_layout.cpp::tokenize_words). An
// isolated invalid byte (GetCodepointNext returns '?' with size 1, see
// raylib) is treated as GlyphFontKind::Base, same as any other printable
// ASCII character.
std::vector<GlyphRun> split_by_glyph_kind(const std::string& text);
