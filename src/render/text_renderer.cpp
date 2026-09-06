#include "render/text_renderer.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <future>
#include <set>
#include <vector>

#include "text/glyph_class.h"

namespace {

// raylib's standard SDF shader: smoothstep over the distance, with the
// smoothing computed via fwidth (screen-space derivatives) for sharp,
// alias-free edges at any drawing scale.
const char* kSdfFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main() {
    float distance = texture(texture0, fragTexCoord).a;
    float smoothing = fwidth(distance);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha) * colDiffuse;
}
)";

// Position/thickness of the strikethrough stroke, as a fraction of the font size.
constexpr float kStrikeYFraction = 0.55f;
constexpr float kStrikeThicknessFraction = 0.07f;

// Padding of the inline code "chip", as a fraction of the font size.
constexpr float kInlineCodePaddingXFraction = 0.12f;
constexpr float kInlineCodePaddingYFraction = 0.10f;
constexpr float kInlineCodeRoundness = 0.35f;
constexpr float kCodeBlockRoundness = 0.10f;

// The codepoint set for every "normal" font (regular/bold/italic/
// bold-italic/mono) comes from text/glyph_class.h — the same source of
// truth used by classify_codepoint to decide whether a character is
// already covered without needing the emoji/Asian fonts (see
// ensure_extra_fonts_loaded).
std::vector<int> build_codepoint_set() { return base_codepoints(); }

// Checks whether `path` (--emoji-font/--asian-font, see cli/cli_args.h)
// points to a real file before trying to load it — without this,
// LoadFileData fails inside rasterize_sdf_font and the only sign would be a
// generic raylib warning ("FILEIO: ... Failed to open file") buried in the
// log, without making clear which of the two parameters is wrong or which
// path was tried. A missing/empty path (parameter not given) produces no
// warning at all — only a non-empty path that doesn't exist does.
std::string validate_extra_font_path(const std::string& path, const char* flag_name) {
  if (path.empty() || FileExists(path.c_str())) {
    return path;
  }
  std::fprintf(stderr,
               "Warning: font given via %s ('%s') not found; ignoring -- characters in that category "
               "will use the regular font (see classify_codepoint).\n",
               flag_name, path.c_str());
  return "";
}

// Result of the heavy part (CPU: reading the file + SDF rasterization via
// stb_truetype + atlas packing) — no OpenGL calls, so this can run on any
// thread. All that's left is uploading `atlas` as a texture.
struct RasterizedFont {
  int base_size;
  int glyph_padding;
  int glyph_count;
  GlyphInfo* glyphs;
  Rectangle* recs;
  Image atlas;
};

RasterizedFont rasterize_sdf_font(std::string font_path, int base_size, std::vector<int> codepoints) {
  RasterizedFont result{};

  int file_size = 0;
  unsigned char* file_data = LoadFileData(font_path.c_str(), &file_size);

  int glyph_count = 0;
  result.base_size = base_size;
  result.glyph_padding = 0;
  result.glyphs = LoadFontData(file_data, file_size, base_size, codepoints.data(),
                                static_cast<int>(codepoints.size()), FONT_SDF, &glyph_count);
  result.glyph_count = glyph_count;

  result.atlas = GenImageFontAtlas(result.glyphs, &result.recs, result.glyph_count, base_size, result.glyph_padding, 1);

  UnloadFileData(file_data);

  return result;
}

// Uploads the rasterized atlas as a texture — must run on the main thread
// (raylib's OpenGL context is not thread-safe).
Font upload_rasterized_font(RasterizedFont& rasterized) {
  Font font{};
  font.baseSize = rasterized.base_size;
  font.glyphPadding = rasterized.glyph_padding;
  font.glyphCount = rasterized.glyph_count;
  font.glyphs = rasterized.glyphs;
  font.recs = rasterized.recs;

  font.texture = LoadTextureFromImage(rasterized.atlas);
  UnloadImage(rasterized.atlas);
  SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

  return font;
}

Font load_sdf_font(const std::string& font_path, int base_size, const std::vector<int>& codepoints) {
  RasterizedFont rasterized = rasterize_sdf_font(font_path, base_size, codepoints);
  return upload_rasterized_font(rasterized);
}

// A variant pending load: rasterization has already been kicked off in
// parallel (`future`); it just needs to be awaited and its texture uploaded
// on the main thread.
struct PendingFontLoad {
  std::future<RasterizedFont> future;
  Font* out_font;
  bool* out_distinct;
  bool* out_loaded;
};

struct PositionedRun {
  const InlineRun* run;
  float x;
  float y;
  float font_size;
  bool is_block_quote;  // true: uses the block highlight color instead of the normal text color
};

struct BackgroundRect {
  Rectangle rect;
  float roundness;
};

struct PositionedImage {
  const Texture2D* texture;
  float x;
  float y;
  float width;
  float height;
};

}  // namespace

TextRenderer load_text_renderer(const FontPaths& font_paths, float base_font_size,
                                 const std::string& emoji_font_path, const std::string& asian_font_path) {
  TextRenderer renderer;

  renderer.font_paths = font_paths;
  renderer.base_size = static_cast<int>(base_font_size);
  renderer.emoji_font_path = validate_extra_font_path(emoji_font_path, "--emoji-font");
  renderer.asian_font_path = validate_extra_font_path(asian_font_path, "--asian-font");

  renderer.bold_loaded = false;
  renderer.italic_loaded = false;
  renderer.bold_italic_loaded = false;
  renderer.mono_loaded = false;
  renderer.emoji_loaded = false;
  renderer.asian_loaded = false;

  renderer.has_distinct_bold_font = false;
  renderer.has_distinct_italic_font = false;
  renderer.has_distinct_bold_italic_font = false;
  renderer.has_distinct_emoji_font = false;
  renderer.has_distinct_asian_font = false;
  renderer.mono_font_is_owned = false;

  // Only the regular one is always needed (every paragraph uses it); the
  // other variants are expensive (each is a 2048x2048 SDF atlas) and are
  // only worth loading if the content actually uses them — see
  // ensure_styles_loaded/ensure_extra_fonts_loaded.
  std::vector<int> codepoints = build_codepoint_set();
  renderer.fonts.regular = load_sdf_font(font_paths.regular, renderer.base_size, codepoints);

  // Until loaded, the other variants stay aliased to the regular one
  // (select_styled_font only picks them if the corresponding style appears
  // in the content, and ensure_styles_loaded/ensure_extra_fonts_loaded run
  // before any layout/drawing — but keeping a valid value here avoids an
  // uninitialized Font in any future use; for emoji/asian, this is also the
  // final behavior when the corresponding font is never given via
  // --emoji-font/--asian-font).
  renderer.fonts.bold = renderer.fonts.regular;
  renderer.fonts.italic = renderer.fonts.regular;
  renderer.fonts.bold_italic = renderer.fonts.regular;
  renderer.fonts.mono = renderer.fonts.regular;
  renderer.fonts.emoji = renderer.fonts.regular;
  renderer.fonts.asian = renderer.fonts.regular;

  renderer.sdf_shader = LoadShaderFromMemory(nullptr, kSdfFragmentShader);

  return renderer;
}

void ensure_styles_loaded(TextRenderer& renderer, const StyleUsage& usage) {
  std::vector<int> codepoints;  // only built if something actually needs to be loaded
  auto codepoints_ready = [&]() -> const std::vector<int>& {
    if (codepoints.empty()) {
      codepoints = build_codepoint_set();
    }
    return codepoints;
  };

  std::vector<PendingFontLoad> pending;

  // Kicks off rasterization (CPU only) of each missing variant in parallel,
  // one thread per font. A variant whose path equals the regular one's
  // (fallback already resolved in resolve_font_paths) doesn't need any
  // thread at all: it just reuses the regular font's texture.
  auto queue_variant = [&](bool needed, bool& loaded_flag, const std::string& path, Font& out_font,
                            bool& out_distinct) {
    if (!needed || loaded_flag) {
      return;
    }
    if (path == renderer.font_paths.regular) {
      out_font = renderer.fonts.regular;
      out_distinct = false;
      loaded_flag = true;
      return;
    }
    pending.push_back(PendingFontLoad{
        std::async(std::launch::async, rasterize_sdf_font, path, renderer.base_size, codepoints_ready()), &out_font,
        &out_distinct, &loaded_flag});
  };

  queue_variant(usage.bold, renderer.bold_loaded, renderer.font_paths.bold, renderer.fonts.bold,
                renderer.has_distinct_bold_font);
  queue_variant(usage.italic, renderer.italic_loaded, renderer.font_paths.italic, renderer.fonts.italic,
                renderer.has_distinct_italic_font);
  queue_variant(usage.bold_italic, renderer.bold_italic_loaded, renderer.font_paths.bold_italic,
                renderer.fonts.bold_italic, renderer.has_distinct_bold_italic_font);

  if (usage.code && !renderer.mono_loaded) {
    if (renderer.font_paths.mono.empty()) {
      renderer.fonts.mono = GetFontDefault();
      renderer.mono_font_is_owned = false;
      renderer.mono_loaded = true;
    } else {
      pending.push_back(PendingFontLoad{std::async(std::launch::async, rasterize_sdf_font, renderer.font_paths.mono,
                                                     renderer.base_size, codepoints_ready()),
                                         &renderer.fonts.mono, &renderer.mono_font_is_owned, &renderer.mono_loaded});
    }
  }

  // Waits for all rasterizations (which ran in parallel) and uploads each
  // texture on the main thread — fast compared to the cost of rasterizing.
  for (PendingFontLoad& load : pending) {
    RasterizedFont rasterized = load.future.get();
    *load.out_font = upload_rasterized_font(rasterized);
    *load.out_distinct = true;
    *load.out_loaded = true;
  }
}

void ensure_extra_fonts_loaded(TextRenderer& renderer, const CodepointUsage& usage) {
  // Warning per codepoint, once per process — avoids repeating the same
  // warning on every hot-reload that still uses the same font-less character.
  static std::set<int> warned_codepoints;

  auto warn_once = [](int codepoint, const char* reason) {
    if (warned_codepoints.insert(codepoint).second) {
      std::fprintf(stderr,
                    "Warning: character U+%04X %s; it will be drawn with the regular font, which doesn't "
                    "have that glyph (shows up as '?').\n",
                    codepoint,
                    reason);
    }
  };

  bool reload_emoji = false;
  if (renderer.emoji_font_path.empty()) {
    for (int codepoint : usage.emoji_codepoints) {
      warn_once(codepoint, "is an emoji, but no font was given (--emoji-font)");
    }
  } else {
    for (int codepoint : usage.emoji_codepoints) {
      if (!renderer.emoji_codepoints_loaded.count(codepoint)) {
        reload_emoji = true;
        break;
      }
    }
  }

  bool reload_asian = false;
  if (renderer.asian_font_path.empty()) {
    for (int codepoint : usage.asian_codepoints) {
      warn_once(codepoint, "is from an Asian script, but no font was given (--asian-font)");
    }
  } else {
    for (int codepoint : usage.asian_codepoints) {
      if (!renderer.asian_codepoints_loaded.count(codepoint)) {
        reload_asian = true;
        break;
      }
    }
  }

  for (int codepoint : usage.other_codepoints) {
    warn_once(codepoint, "is not supported by the default font and has no dedicated font available");
  }

  if (!reload_emoji && !reload_asian) {
    return;  // nothing new to load — the common case (no emoji/Asian text, or already covered by the current atlas)
  }

  // Kicks off rasterization (CPU only) of the fonts that need a reload in
  // parallel, just like ensure_styles_loaded — the atlas grows with the set
  // of codepoints seen so far (union, never shrinks), so a hot-reload that
  // removes an emoji doesn't force a pointless reload.
  std::future<RasterizedFont> emoji_future;
  if (reload_emoji) {
    renderer.emoji_codepoints_loaded.insert(usage.emoji_codepoints.begin(), usage.emoji_codepoints.end());
    std::vector<int> codepoints(renderer.emoji_codepoints_loaded.begin(), renderer.emoji_codepoints_loaded.end());
    emoji_future = std::async(std::launch::async, rasterize_sdf_font, renderer.emoji_font_path, renderer.base_size,
                               std::move(codepoints));
  }

  std::future<RasterizedFont> asian_future;
  if (reload_asian) {
    renderer.asian_codepoints_loaded.insert(usage.asian_codepoints.begin(), usage.asian_codepoints.end());
    std::vector<int> codepoints(renderer.asian_codepoints_loaded.begin(), renderer.asian_codepoints_loaded.end());
    asian_future = std::async(std::launch::async, rasterize_sdf_font, renderer.asian_font_path, renderer.base_size,
                               std::move(codepoints));
  }

  if (reload_emoji) {
    RasterizedFont rasterized = emoji_future.get();
    Font new_font = upload_rasterized_font(rasterized);
    if (renderer.has_distinct_emoji_font) {
      UnloadFont(renderer.fonts.emoji);
    }
    renderer.fonts.emoji = new_font;
    renderer.has_distinct_emoji_font = true;
    renderer.emoji_loaded = true;
  }

  if (reload_asian) {
    RasterizedFont rasterized = asian_future.get();
    Font new_font = upload_rasterized_font(rasterized);
    if (renderer.has_distinct_asian_font) {
      UnloadFont(renderer.fonts.asian);
    }
    renderer.fonts.asian = new_font;
    renderer.has_distinct_asian_font = true;
    renderer.asian_loaded = true;
  }
}

void unload_text_renderer(TextRenderer& renderer) {
  UnloadFont(renderer.fonts.regular);
  if (renderer.has_distinct_bold_font) {
    UnloadFont(renderer.fonts.bold);
  }
  if (renderer.has_distinct_italic_font) {
    UnloadFont(renderer.fonts.italic);
  }
  if (renderer.has_distinct_bold_italic_font) {
    UnloadFont(renderer.fonts.bold_italic);
  }
  if (renderer.has_distinct_emoji_font) {
    UnloadFont(renderer.fonts.emoji);
  }
  if (renderer.has_distinct_asian_font) {
    UnloadFont(renderer.fonts.asian);
  }
  if (renderer.mono_font_is_owned) {
    UnloadFont(renderer.fonts.mono);
  }
  UnloadShader(renderer.sdf_shader);
}

void draw_centered_text(const TextRenderer& renderer, const TextLayoutResult& layout, int window_width,
                         int window_height, const AppConfig& config, const SlideParams& params) {
  Color text_color = params.text_color.value_or(config.text_color);
  Color code_background_color = params.block_color.value_or(config.code_background_color);

  std::vector<PositionedRun> positioned;
  std::vector<BackgroundRect> backgrounds;    // code block panels (background, drawn first)
  std::vector<BackgroundRect> inline_chips;   // inline code chips
  std::vector<Rectangle> strikes;             // strikethrough lines (drawn on top of the text)
  std::vector<Rectangle> rules;               // horizontal rules (---)
  std::vector<Rectangle> quote_bars;          // vertical blockquote bars (>)
  std::vector<PositionedImage> images;        // successfully loaded images
  std::vector<Rectangle> table_grid_lines;    // row/column borders and outer outline of tables
  // Synthetic runs for list item markers ("•", "1.", "ii.", ...) — a deque
  // instead of a vector because `positioned` keeps a pointer to each one
  // (see below); a vector could reallocate and invalidate those pointers on
  // every new item, a deque never moves elements already inserted.
  std::deque<InlineRun> list_marker_runs;

  // Horizontal position of a block/line of width `width`, according to
  // `params.align` — same centering formula as before when it's Center, so
  // the default (no "align" in the marker) stays identical to the behavior
  // that predates this parameter.
  auto align_x = [&](float width) {
    switch (params.align) {
      case TextAlign::Left:
        return config.margin_x;
      case TextAlign::Right:
        return static_cast<float>(window_width) - config.margin_x - width;
      case TextAlign::Center:
      default:
        return (static_cast<float>(window_width) - width) / 2.0f;
    }
  };

  float start_y = (static_cast<float>(window_height) - layout.block_height) / 2.0f;
  float y = start_y;

  for (const LayoutBlock& block : layout.blocks) {
    float code_padding = block.is_code_block ? block.font_size * kCodeBlockPaddingFraction : 0.0f;
    y += code_padding;

    float block_start_y = y;
    float code_box_width = 0.0f;
    if (block.is_code_block) {
      for (const TextLine& line : block.lines) {
        code_box_width = std::max(code_box_width, line.width);
      }
    }
    float code_box_left_x = align_x(code_box_width);

    // Blockquote indentation: width of the vertical bar + gap to the text
    // (see kBlockQuoteBarWidthFraction/kBlockQuoteGapFraction), a sum
    // already reflected in layout_block.lines by text_layout.cpp (shorter
    // lines, available for wrapping). The "box" (bar + text) is aligned as
    // a unit, just like the code block panel.
    float quote_indent = block.is_block_quote ? block.font_size * (kBlockQuoteBarWidthFraction +
                                                                     kBlockQuoteGapFraction)
                                               : 0.0f;
    float quote_box_width = 0.0f;
    if (block.is_block_quote) {
      for (const TextLine& line : block.lines) {
        quote_box_width = std::max(quote_box_width, line.width);
      }
      quote_box_width += quote_indent;
    }
    float quote_box_left_x = align_x(quote_box_width);

    // List item indentation: recomputed from block.list_indent
    // (level * step) + the marker's width at its own size
    // (block.list_marker_font_size — smaller than block.font_size only for
    // the hollow circle marker, see kListCircleMarkerScale), both already
    // computed by text_layout.cpp. list_box_width also comes from there
    // ready-made — it's shared by every list item in the slide (see the
    // pass at the end of layout_at_font_size), not recomputed from this
    // single item's lines, so that items (and the slide's lists relative to
    // each other) line up in a column, instead of each aligning on its own.
    float list_text_indent = 0.0f;
    float list_box_width = 0.0f;
    if (block.is_list_item) {
      float marker_spacing = block.list_marker_font_size * kLetterSpacingFraction;
      float marker_width = MeasureTextEx(renderer.fonts.regular, block.list_marker.c_str(),
                                          block.list_marker_font_size, marker_spacing)
                                .x;
      list_text_indent = block.list_indent + marker_width + block.font_size * kListMarkerGapFraction;
      list_box_width = block.list_box_width;
    }
    float list_box_left_x = align_x(list_box_width);

    for (size_t line_index = 0; line_index < block.lines.size(); ++line_index) {
      const TextLine& line = block.lines[line_index];

      if (block.is_thematic_break) {
        float rule_x = align_x(line.width);
        float rule_y = y + block.line_height * 0.5f;
        rules.push_back(Rectangle{rule_x, rule_y, line.width, block.font_size * kThematicBreakThicknessFraction});
        y += block.line_height;
        continue;
      }

      if (block.is_table) {
        // A table isn't drawn line by line here — `line` is just an empty
        // TextLine reserving the height of one table row (see
        // layout_at_font_size). The actual drawing (cells, header
        // background, grid) happens all at once after this loop, with
        // block_start_y and the total width/height already defined.
        y += block.line_height;
        continue;
      }

      float x;
      if (block.is_code_block) {
        x = code_box_left_x;
      } else if (block.is_block_quote) {
        x = quote_box_left_x + quote_indent;
      } else if (block.is_list_item) {
        x = list_box_left_x + list_text_indent;
      } else {
        x = align_x(line.width);
      }

      // The item marker ("•", "1.", ...) is only drawn on the first
      // line — hanging indentation: following lines (wrapped text) align
      // with the start of the text, they don't repeat the marker.
      if (block.is_list_item && line_index == 0 && !block.list_marker.empty()) {
        float marker_x = list_box_left_x + block.list_indent;
        // Vertically centers the marker on the line when it's drawn smaller
        // than the text (hollow circle, see kListCircleMarkerScale) —
        // otherwise it would stick to the top of the line, higher than the
        // rest of the text beside it.
        float marker_y = y + (block.font_size - block.list_marker_font_size) * 0.5f;
        list_marker_runs.push_back(InlineRun{block.list_marker, 0.0f, false, false, false, false});
        positioned.push_back(PositionedRun{&list_marker_runs.back(), marker_x, marker_y, block.list_marker_font_size,
                                            false});
      }

      // Successfully loaded image: the single "line" reserved for it (see
      // layout_at_font_size) has no text runs — `x` above is already the
      // right position, because line.width is image_width itself. Without
      // a texture (nullptr), `line` already carries the alt text wrapped
      // like a regular paragraph, drawn normally by the runs loop below.
      if (block.is_image && block.image_texture != nullptr) {
        images.push_back(PositionedImage{block.image_texture, x, y, block.image_width, block.image_height});
      }

      for (const InlineRun& run : line.runs) {
        positioned.push_back(PositionedRun{&run, x, y, block.font_size, block.is_block_quote});

        if (run.code && !block.is_code_block && run.width > 0.0f) {
          float pad_x = block.font_size * kInlineCodePaddingXFraction;
          float pad_y = block.font_size * kInlineCodePaddingYFraction;
          inline_chips.push_back(BackgroundRect{
              Rectangle{x - pad_x, y - pad_y, run.width + 2.0f * pad_x, block.font_size + 2.0f * pad_y},
              kInlineCodeRoundness});
        }

        x += run.width;
      }

      y += block.line_height;
    }

    if (block.is_code_block) {
      backgrounds.push_back(BackgroundRect{
          Rectangle{code_box_left_x - code_padding, block_start_y - code_padding,
                    code_box_width + 2.0f * code_padding, (y - block_start_y) + 2.0f * code_padding},
          kCodeBlockRoundness});
    }
    if (block.is_block_quote) {
      float bar_width = block.font_size * kBlockQuoteBarWidthFraction;
      quote_bars.push_back(Rectangle{quote_box_left_x, block_start_y, bar_width, y - block_start_y});
    }
    if (block.is_table) {
      float table_width = 0.0f;
      for (float column_width : block.table_column_width) {
        table_width += column_width;
      }
      float table_left_x = align_x(table_width);
      float table_top_y = block_start_y;
      float table_height = y - block_start_y;
      float border = block.font_size * kTableBorderThicknessFraction;
      float cell_padding_x = block.font_size * kTableCellPaddingXFraction;
      float text_line_height = block.font_size * config.line_height_multiplier;
      float text_padding_y = (block.table_row_height - text_line_height) * 0.5f;

      // Positions one whole row (header or body): each cell aligned within
      // its own column according to block.table_column_align[col] (default:
      // left).
      auto place_row = [&](const std::vector<TextLine>& row, float row_top_y) {
        float column_x = table_left_x;
        for (size_t col = 0; col < block.table_column_width.size(); ++col) {
          float column_width = block.table_column_width[col];
          if (col < row.size()) {
            const TextLine& cell = row[col];
            TextAlign align = col < block.table_column_align.size() ? block.table_column_align[col] : TextAlign::Left;
            float cell_x;
            if (align == TextAlign::Center) {
              cell_x = column_x + (column_width - cell.width) * 0.5f;
            } else if (align == TextAlign::Right) {
              cell_x = column_x + column_width - cell.width - cell_padding_x;
            } else {
              cell_x = column_x + cell_padding_x;
            }
            float run_x = cell_x;
            for (const InlineRun& run : cell.runs) {
              positioned.push_back(PositionedRun{&run, run_x, row_top_y + text_padding_y, block.font_size, false});
              run_x += run.width;
            }
          }
          column_x += column_width;
        }
      };

      place_row(block.table_header_row, table_top_y);
      for (size_t r = 0; r < block.table_body_rows.size(); ++r) {
        place_row(block.table_body_rows[r], table_top_y + block.table_row_height * static_cast<float>(r + 1));
      }

      // Header background (same code block/blockquote highlight color — see
      // code_background_color) and the grid: outer outline + borders
      // between columns and between rows, all in the text color.
      backgrounds.push_back(
          BackgroundRect{Rectangle{table_left_x, table_top_y, table_width, block.table_row_height}, 0.0f});

      size_t total_rows = 1 + block.table_body_rows.size();
      float column_x = table_left_x;
      for (size_t col = 0; col + 1 < block.table_column_width.size(); ++col) {
        column_x += block.table_column_width[col];
        table_grid_lines.push_back(Rectangle{column_x - border * 0.5f, table_top_y, border, table_height});
      }
      for (size_t r = 0; r <= total_rows; ++r) {
        float row_y = table_top_y + block.table_row_height * static_cast<float>(r);
        table_grid_lines.push_back(Rectangle{table_left_x, row_y - border * 0.5f, table_width, border});
      }
      table_grid_lines.push_back(Rectangle{table_left_x - border * 0.5f, table_top_y, border, table_height});
      table_grid_lines.push_back(
          Rectangle{table_left_x + table_width - border * 0.5f, table_top_y, border, table_height});
    }

    y += code_padding;
    y += block.spacing_after;
  }

  for (const BackgroundRect& bg : backgrounds) {
    DrawRectangleRounded(bg.rect, bg.roundness, 8, code_background_color);
  }
  for (const BackgroundRect& chip : inline_chips) {
    DrawRectangleRounded(chip.rect, chip.roundness, 8, code_background_color);
  }
  for (const Rectangle& rule : rules) {
    DrawLineEx(Vector2{rule.x, rule.y}, Vector2{rule.x + rule.width, rule.y}, rule.height, text_color);
  }
  for (const Rectangle& bar : quote_bars) {
    DrawRectangleRec(bar, code_background_color);
  }
  for (const Rectangle& line : table_grid_lines) {
    DrawRectangleRec(line, text_color);
  }
  // Deliberately outside the BeginShaderMode/EndShaderMode below: the
  // shader there is the text SDF one (smoothstep over a distance field in
  // the alpha channel, see kSdfFragmentShader), not a normal image shader —
  // applied to a regular RGBA texture, it would corrupt the colors.
  for (const PositionedImage& image : images) {
    Rectangle source{0.0f, 0.0f, static_cast<float>(image.texture->width), static_cast<float>(image.texture->height)};
    Rectangle dest{image.x, image.y, image.width, image.height};
    DrawTexturePro(*image.texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
  }

  BeginShaderMode(renderer.sdf_shader);
  for (const PositionedRun& p : positioned) {
    const InlineRun& run = *p.run;
    const Font& font = select_styled_font(run.bold, run.italic, run.code, run.kind, renderer.fonts);
    float spacing = p.font_size * kLetterSpacingFraction;
    // Inline code inside a blockquote keeps the normal text color, even
    // with is_block_quote — otherwise it would end up the same color as its
    // own background chip (also code_background_color) and the text would
    // disappear into it.
    Color run_color = (p.is_block_quote && !run.code) ? code_background_color : text_color;
    DrawTextEx(font, run.text.c_str(), Vector2{p.x, p.y}, p.font_size, spacing, run_color);

    if (run.strikethrough && run.width > 0.0f) {
      strikes.push_back(Rectangle{p.x, p.y + p.font_size * kStrikeYFraction, run.width,
                                   p.font_size * kStrikeThicknessFraction});
    }
  }
  EndShaderMode();

  for (const Rectangle& strike : strikes) {
    DrawLineEx(Vector2{strike.x, strike.y}, Vector2{strike.x + strike.width, strike.y}, strike.height, text_color);
  }
}
