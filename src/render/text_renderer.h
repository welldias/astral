#pragma once

#include <raylib.h>

#include <set>
#include <string>

#include "markdown/markdown_parser.h"
#include "platform/default_font.h"
#include "slides/slide_params.h"
#include "text/text_layout.h"

struct TextRenderer {
  FontSet fonts;  // fonts.regular is always loaded; the others only if requested via ensure_styles_loaded

  FontPaths font_paths;  // resolved paths, kept to load on demand
  int base_size;

  // Paths passed via --emoji-font/--asian-font (see cli/cli_args.h);
  // empty = parameter not given. Kept here (instead of only in
  // CliArgs) so ensure_extra_fonts_loaded can decide, on every
  // document reload (hot-reload), whether it needs to load on demand.
  std::string emoji_font_path;
  std::string asian_font_path;

  bool bold_loaded;
  bool italic_loaded;
  bool bold_italic_loaded;
  bool mono_loaded;
  bool emoji_loaded;
  bool asian_loaded;

  // true: the corresponding variant was loaded as its own texture
  // (needs UnloadFont); false: never requested, or it's the same font as
  // `fonts.regular` (fallback already resolved in resolve_font_paths, or
  // --emoji-font/--asian-font not given).
  bool has_distinct_bold_font;
  bool has_distinct_italic_font;
  bool has_distinct_bold_italic_font;
  bool has_distinct_emoji_font;
  bool has_distinct_asian_font;

  // Codepoints already rasterized in the current fonts.emoji/fonts.asian
  // atlas — used by ensure_extra_fonts_loaded to know whether a hot-reload
  // introduced a new codepoint (in that case the atlas needs to be
  // reloaded; the set only grows, never shrinks).
  std::set<int> emoji_codepoints_loaded;
  std::set<int> asian_codepoints_loaded;

  // false: fonts.mono was never requested, or it's raylib's built-in font
  // (GetFontDefault(), which raylib manages itself) — do not unload it.
  bool mono_font_is_owned;

  Shader sdf_shader;
};

// Loads only the regular font (SDF, smooth edges at any scale,
// baked at `base_font_size`) — always needed. The other variants are only
// loaded when requested via ensure_styles_loaded/ensure_extra_fonts_loaded,
// so as not to pay the cost of generating an SDF atlas when the content
// doesn't use bold/italic/code/emoji/Asian characters. `emoji_font_path`/
// `asian_font_path` come from --emoji-font/--asian-font (see cli/cli_args.h)
// — empty when the corresponding parameter wasn't passed.
TextRenderer load_text_renderer(const FontPaths& font_paths, float base_font_size,
                                 const std::string& emoji_font_path, const std::string& asian_font_path);

// Ensures each variant marked in `usage` is loaded, loading on demand
// whichever is still missing (idempotent: already loaded doesn't reload).
// Call after parse_markdown, both on the initial load and on every
// reload — the content may start using a style it didn't use before.
void ensure_styles_loaded(TextRenderer& renderer, const StyleUsage& usage);

// Ensures the emoji/Asian font is loaded with the codepoints from `usage`
// (see markdown/markdown_parser.h::CodepointUsage), if
// `renderer.emoji_font_path`/`asian_font_path` was given — loads only the
// codepoints actually used in the document (not a generic range), so the
// atlas grows with the content, not with the font's coverage. Idempotent
// like ensure_styles_loaded: a reload (hot-reload) that introduces a new
// codepoint triggers a new load replacing the previous atlas (it may not
// cover the new codepoint); a document that only uses codepoints already
// covered reloads nothing.
//
// For each codepoint in `usage.emoji_codepoints`/`asian_codepoints` with no
// corresponding font given, and for each codepoint in
// `usage.other_codepoints` (a script with no dedicated font possible), logs
// a warning to stderr the first time it appears (per process) and continues
// rendering — the character is drawn with the regular font, which doesn't
// have the glyph (shows up as '?', see raylib::GetGlyphIndex), without
// crashing or interrupting the program.
void ensure_extra_fonts_loaded(TextRenderer& renderer, const CodepointUsage& usage);

void unload_text_renderer(TextRenderer& renderer);

// Draws `layout` vertically centered in the window; the horizontal
// position follows `params.align` (default for a slide with no "align"
// parameter in the marker: TextAlign::Center) — code blocks keep their text
// left-aligned inside the panel itself regardless of `params.align`, but
// the panel itself is positioned horizontally like any other block. The
// text color and the inline/block code highlight background follow
// `params.text_color`/`params.block_color` when the slide provides them
// (via the "$[...]" marker), falling back to `config.text_color`/
// `config.code_background_color` otherwise — `params.bg_color` is not used
// here, since clearing the screen is the caller's responsibility (see
// main.cpp).
void draw_centered_text(const TextRenderer& renderer, const TextLayoutResult& layout, int window_width,
                         int window_height, const AppConfig& config, const SlideParams& params);
