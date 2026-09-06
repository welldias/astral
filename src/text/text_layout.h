#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "assets/image_cache.h"
#include "config/config.h"
#include "markdown/markdown_parser.h"
#include "text/glyph_class.h"

// Padding of a code block's "panel", as a fraction of font_size. Shared
// between the layout computation (text_layout.cpp) and the background
// rectangle drawing (render/text_renderer.cpp) — must be the same value in
// both places so the background doesn't spill into the next block or leave
// dead space.
inline constexpr float kCodeBlockPaddingFraction = 0.5f;

// Thickness of the thematic break (---) stroke, as a fraction of font_size
// — also the height of the "line" reserved for it (just the stroke itself,
// no extra padding: the spacing above/below the rule is entirely
// controlled by AppConfig::horizontal_rule_top_spacing/paragraph_spacing).
// Shared between text_layout.cpp (reserves the space) and
// render/text_renderer.cpp (draws the stroke).
inline constexpr float kThematicBreakThicknessFraction = 0.06f;

// Width of a blockquote's (BlockKind::BlockQuote) vertical bar and the gap
// between it and the text, both as fractions of font_size. Together they
// form the indentation reserved on the left of the quote's text — shared
// between text_layout.cpp (reduces the width available for line wrapping
// and adds to the block's width) and render/text_renderer.cpp (draws the
// bar and offsets the text).
inline constexpr float kBlockQuoteBarWidthFraction = 0.12f;
inline constexpr float kBlockQuoteGapFraction      = 0.6f;

// Indentation of a list item (BlockKind::ListItem), as fractions of
// font_size: kListIndentPerLevelFraction advances per nesting level
// (ContentBlock::list_level), kListMarkerGapFraction is the gap between the
// marker ("•", "1.", "ii.", "a.", ...) and the item's text. Shared between
// text_layout.cpp (reduces the width available for line wrapping and adds
// to the block's width) and render/text_renderer.cpp (draws the marker and
// offsets the text).
inline constexpr float kListIndentPerLevelFraction = 1.3f;
inline constexpr float kListMarkerGapFraction      = 0.5f;

// The hollow-circle glyph ("○", 2nd-level unordered list marker — see
// list_marker_text in text_layout.cpp) is drawn noticeably larger than the
// other markers at the font's normal size; this fraction shrinks just that
// marker to visually balance it with the other levels. Shared between
// text_layout.cpp and render/text_renderer.cpp via the
// LayoutBlock::list_marker_font_size field, computed once during layout
// and reused (not recomputed) when drawing.
inline constexpr float kListCircleMarkerScale = 0.4f;

// Drawing size of an image (BlockKind::Image): the largest scale, keeping
// the original aspect ratio, that fits within both kImageMaxWidthFraction
// of max_width and kImageMaxHeightFraction of available_height (same as
// CSS's "object-fit: contain") — never upscaled beyond the file's native
// size (see layout_at_font_size, in text_layout.cpp). These leave a visual
// margin even when the image is the slide's only content, and guarantee
// that an image alone never forces the rest of the slide's font to shrink
// (it already fits within the available space from the start).
inline constexpr float kImageMaxWidthFraction  = 1.0f;
inline constexpr float kImageMaxHeightFraction = 0.85f;

// Geometry of a table (BlockKind::Table), as fractions of font_size:
// kTableCellPaddingXFraction is the horizontal breathing room between a
// cell's text and the column border (on both sides); kTableCellPaddingYFraction
// is the vertical breathing room between the text and the row's top/bottom
// borders (added to the text line's height to give each table row's
// height); kTableBorderThicknessFraction is the thickness of the grid
// lines (row/column borders) and the outer outline. Shared between
// text_layout.cpp (measures columns/rows) and render/text_renderer.cpp
// (draws the grid and positions each cell's text).
inline constexpr float kTableCellPaddingXFraction    = 0.6f;
inline constexpr float kTableCellPaddingYFraction    = 0.45f;
inline constexpr float kTableBorderThicknessFraction = 0.035f;

// Letter-spacing added between consecutive characters when measuring/
// drawing text (raylib's MeasureTextEx/DrawTextEx `spacing` parameter),
// as a fraction of font_size. Shared between text_layout.cpp (measuring
// words while wrapping/reflowing) and render/text_renderer.cpp (the actual
// DrawTextEx calls, plus list item markers) — must be the same value in
// both places so a run is drawn exactly as wide as it was measured.
//
// This only ever applies WITHIN a run of consecutive characters (a word, a
// run of code, a marker) — a run boundary (e.g. the space between two
// words) is its own single-character run (see the space handling in
// text_layout.cpp's word-wrapping loop) and gets none of this extra
// spacing added on top of its own glyph advance. A value that's too large
// relative to the font's own space-glyph width makes the gaps between
// letters of the same word approach the gap between two words, blurring
// where one word ends and the next begins.
inline constexpr float kLetterSpacingFraction = 0.03f;

struct FontSet {
    Font regular;
    Font bold;
    Font italic;
    Font bold_italic;
    Font mono;

    // Dedicated fonts for emoji/Asian-language characters, passed via
    // --emoji-font/--asian-font (see cli/cli_args.h). While not loaded
    // (parameter absent, or no usage detected in the document — see
    // render/text_renderer.h::ensure_extra_fonts_loaded), they stay aliased
    // to `regular`, same scheme as bold/italic in TextRenderer::fonts.
    Font emoji;
    Font asian;
};

struct InlineRun {
    std::string text;
    float width;
    bool bold;
    bool italic;
    bool strikethrough;
    bool code;

    // Font category for this run's text (see text/glyph_class.h) — regular
    // text runs are partitioned by GlyphFontKind in tokenize_words, so each
    // span (emoji/Asian/rest) draws with the right font even within the same
    // word. `code` takes priority over `kind` in select_styled_font: code
    // always uses the mono font, not the emoji/Asian one, even if it
    // contains those characters.
    GlyphFontKind kind = GlyphFontKind::Base;
};

struct TextLine {
    std::vector<InlineRun> runs;
    float width;
};

struct LayoutBlock {
    std::vector<TextLine> lines;
    float font_size;
    float line_height;
    float spacing_after;    // extra space after this block (0 on the last block)
    bool is_code_block;     // true: lines left-aligned inside their own panel, no reflow
    bool is_thematic_break; // true: no text — a single "line" that is the horizontal rule itself
    bool is_block_quote;    // true: forced italic, indented by a vertical bar on the left

    // Valid only when is_list_item == true. list_marker is the item's marker
    // text ("•", "1.", "ii.", "a.", ...), drawn only on the first line
    // (hanging indent — subsequent lines, if the text wraps, align with the
    // start of the text, not the marker). list_marker_font_size is the size
    // at which list_marker is drawn — same as font_size, except for the
    // hollow circle, which uses kListCircleMarkerScale (see above).
    // list_indent is the marker's horizontal offset from the start of the
    // block (list_level * font_size * kListIndentPerLevelFraction).
    // list_box_width is the width of the "box" (marker + text) used to align
    // the item — it's not this item's own width: it's shared by ALL list
    // items on the slide, even ones from different lists separated by other
    // content (see the step at the end of layout_at_font_size, in
    // text_layout.cpp), so items line up in a column as a single list and
    // the slide's lists line up with each other, instead of each item (or
    // each list) centering/aligning on its own.
    bool is_list_item;
    std::string list_marker;
    float list_marker_font_size;
    float list_indent;
    float list_box_width;

    // Valid only when is_image == true. image_texture is the already-loaded
    // texture (see assets/image_cache.h), or nullptr if the file wasn't
    // found/loaded — in that case `lines` carries the alt text
    // (ContentBlock::spans) wrapped as a normal paragraph, instead of a
    // single empty "line" sized to the image (see layout_at_font_size), and
    // that's what should be drawn, not image_width/image_height (which are
    // left at their default, meaningless value in that case). image_texture
    // points into the ImageCache passed to compute_fitted_layout — valid
    // until unload_image_cache, never invalidated by other images loaded
    // after it.
    bool is_image;
    const Texture2D *image_texture;
    float image_width;
    float image_height;

    // Valid only when is_table == true. A table doesn't reflow: each cell is
    // a single TextLine (no line wrapping of its own — see
    // layout_at_font_size), and every row in the table has the same height
    // (table_row_height). table_column_width[i] already includes the
    // horizontal padding on both sides (kTableCellPaddingXFraction),
    // table_column_align is a copy of ContentBlock::table_column_align
    // (decides the text alignment within the column, at draw time).
    // table_header_row has one cell per column; table_body_rows[row][column]
    // is the rest — a body row can have fewer cells than columns (a missing
    // cell becomes an empty TextLine, see text_layout.cpp), never more.
    //
    // `lines` (inherited from every LayoutBlock) isn't used for the table's
    // text itself — it has one empty TextLine per table row (header + body),
    // just so the generic block width/height computation below can reuse the
    // same formula the other block types already use (see
    // layout_at_font_size); what actually draws it is
    // render/text_renderer.cpp, using the fields below.
    bool is_table;
    std::vector<float> table_column_width;
    std::vector<TextAlign> table_column_align;
    std::vector<TextLine> table_header_row;
    std::vector<std::vector<TextLine>> table_body_rows;
    float table_row_height;
};

struct TextLayoutResult {
    std::vector<LayoutBlock> blocks;
    float block_width;  // largest line width (or code panel width) among all blocks
    float block_height; // total height, already summing inter-block spacing and code block padding
};

// Picks, among the fonts in `fonts`, the right variant for the given
// style. Priority: `code` (only one monospace font exists) > `kind` != Base
// (emoji/Asian always use the corresponding dedicated font, ignoring
// bold/italic — those fonts only have one weight) > bold/italic of the
// "normal" font.
const Font &select_styled_font(bool bold, bool italic, bool code, GlyphFontKind kind, const FontSet &fonts);

// Computes the layout of `content` (already-parsed Markdown blocks).
// Paragraphs, headings, blockquotes and list items are wrapped into lines
// word by word, with reflow (heading forces bold, see
// AppConfig::heading_scale_h1..h4; blockquote forces italic and reserves
// kBlockQuoteBarWidthFraction + kBlockQuoteGapFraction of font_size as
// left indentation, for the vertical bar drawn by
// render/text_renderer.cpp; list item reserves list_level *
// kListIndentPerLevelFraction + kListMarkerGapFraction of font_size, for
// the nesting level and the marker). Code blocks preserve each line of the
// original text literally (indentation/internal spaces included) and are
// left-aligned within their own panel — they don't reflow, and can only
// overflow horizontally in extreme cases (accepted, triggers the same
// font-shrink below). A thematic break has no text; the space between it
// and the previous block uses config.horizontal_rule_top_spacing (smaller
// than the normal spacing), to sit closer to the text above. An image uses
// `image_cache` to load (or reuse, if already loaded) the texture and read
// its dimensions, and is sized only once (it doesn't shrink along with the
// font in the auto-shrink below — see
// kImageMaxWidthFraction/kImageMaxHeightFraction, it's already sized to
// fit within the available space from the start); without the image, it
// falls back to the alt text, treated as a normal paragraph. A table
// doesn't reflow (each cell is a single line, no wrapping) and the header
// is always drawn in bold, like the headings.
//
// Starts at the paragraph font size `initial_font_size` and shrinks that
// size in steps — keeping the proportion between paragraph, headings and
// blockquote (config.blockquote_scale) — until the block fits within
// `available_width` x `available_height`, or until it reaches
// config.min_font_size (floor — residual overflow is accepted in that
// case).
TextLayoutResult compute_fitted_layout(const std::vector<ContentBlock> &content, const FontSet &fonts, const AppConfig &config, float initial_font_size, float available_width, float available_height, ImageCache &image_cache);
