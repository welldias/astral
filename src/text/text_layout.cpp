#include "text/text_layout.h"

#include <algorithm>
#include <cctype>

#include "render/mermaid_render.h"

namespace {

constexpr float kFontShrinkStep = 2.0f;

float text_spacing(float font_size) {
    return font_size * kLetterSpacingFraction;
}

float heading_scale(const AppConfig &config, int heading_level) {
    switch (heading_level) {
    case 1:
        return config.heading_scale_h1;
    case 2:
        return config.heading_scale_h2;
    case 3:
        return config.heading_scale_h3;
    default:
        return config.heading_scale_h4;
    }
}

// "a", "b", ..., "z", "aa", "ab", ... (base 26, no zero digit) — ordered
// list marker at the 3rd nesting level (see list_marker_text).
std::string lower_alpha_marker(int index) {
    std::string result;
    int n = index;
    while (n > 0) {
        int remainder = (n - 1) % 26;
        result        = static_cast<char>('a' + remainder) + result;
        n             = (n - 1) / 26;
    }
    return result;
}

// Lowercase Roman numeral — ordered list marker at the 2nd nesting level.
// Covers a realistic item range for a slide (doesn't validate an upper
// bound; an invalid index, e.g. <= 0, returns an empty string).
std::string lower_roman_marker(int index) {
    static constexpr struct {
        int value;
        const char *symbol;
    } kNumerals[] = {
        { 1000, "m"  },
        { 900,  "cm" },
        { 500,  "d"  },
        { 400,  "cd" },
        { 100,  "c"  },
        { 90,   "xc" },
        { 50,   "l"  },
        { 40,   "xl" },
        { 10,   "x"  },
        { 9,    "ix" },
        { 5,    "v"  },
        { 4,    "iv" },
        { 1,    "i"  },
    };

    std::string result;
    int n = index;
    for (const auto &numeral : kNumerals) {
        while (n >= numeral.value) {
            result += numeral.symbol;
            n -= numeral.value;
        }
    }
    return result;
}

// Marker text for a list item, per ContentBlock::list_ordered and
// list_level: unordered lists alternate solid/hollow/solid marker per
// level ("•"/"○"/"▪", cycling every 3); ordered ones alternate the
// numbering "alphabet" (arabic/lowercase roman/lowercase letter, cycling
// the same way) — the same pattern browsers use for nested HTML lists.
std::string list_marker_text(const ContentBlock &block) {
    int level_in_cycle = block.list_level % 3;

    if (!block.list_ordered) {
        switch (level_in_cycle) {
        case 0:
            return "•"; // • BULLET
        case 1:
            return "○"; // ○ WHITE CIRCLE
        default:
            return "▪"; // ▪ BLACK SMALL SQUARE
        }
    }

    switch (level_in_cycle) {
    case 0:
        return std::to_string(block.list_index) + ".";
    case 1:
        return lower_roman_marker(block.list_index) + ".";
    default:
        return lower_alpha_marker(block.list_index) + ".";
    }
}

// Size, as a fraction of font_size, at which list_marker_text(block) should
// be drawn — 1.0 for most, except the hollow circle (2nd-level unordered
// list), which in the font used comes out noticeably larger than the other
// markers at normal size (see kListCircleMarkerScale).
float list_marker_font_scale(const ContentBlock &block) {
    bool is_circle_marker = !block.list_ordered && (block.list_level % 3) == 1;
    return is_circle_marker ? kListCircleMarkerScale : 1.0f;
}

// A word "run": a space-free text fragment, with the style of the span it
// came from. A word can have more than one run when a style starts or ends
// in the middle of it (e.g. "**bold**rest", with no space between the two).
struct WordRun {
    std::string text;
    bool bold;
    bool italic;
    bool strikethrough;
    bool code;
    GlyphFontKind kind = GlyphFontKind::Base;
};

using Word = std::vector<WordRun>;

std::vector<Word> tokenize_words(const std::vector<TextSpan> &spans, bool force_bold, bool force_italic) {
    std::vector<Word> words;
    Word current;

    auto flush = [&]() {
        if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    };

    for (const TextSpan &span : spans) {
        bool bold   = span.bold || force_bold;
        bool italic = span.italic || force_italic;

        if (span.code) {
            // Inline code preserves internal spaces literally and never breaks
            // in the middle: the whole span becomes a single run, without
            // scanning for spaces.
            if (!span.text.empty()) {
                current.push_back(WordRun{ span.text, bold, italic, span.strikethrough, true });
            }
            continue;
        }

        size_t i = 0;
        while (i < span.text.size()) {
            if (std::isspace(static_cast<unsigned char>(span.text[i]))) {
                flush();
                ++i;
                continue;
            }
            size_t start = i;
            while (i < span.text.size() && !std::isspace(static_cast<unsigned char>(span.text[i]))) {
                ++i;
            }
            // A space-free chunk can mix regular text with emoji/Asian
            // characters (e.g. "Olá👋" or "café中文") — partitions by
            // GlyphFontKind so each piece can be drawn with the right font (see
            // select_styled_font), even within the same word/style run.
            for (GlyphRun &glyph_run : split_by_glyph_kind(span.text.substr(start, i - start))) {
                current.push_back(WordRun{ std::move(glyph_run.text), bold, italic, span.strikethrough, false, glyph_run.kind });
            }
        }
    }
    flush();

    return words;
}

float measure_run(const WordRun &run, const FontSet &fonts, float font_size, float spacing) {
    const Font &font = select_styled_font(run.bold, run.italic, run.code, run.kind, fonts);
    return MeasureTextEx(font, run.text.c_str(), font_size, spacing).x;
}

void append_word(std::vector<InlineRun> &runs, float &width, const Word &word, const FontSet &fonts, float font_size, float spacing) {
    for (const WordRun &run : word) {
        float run_width = measure_run(run, fonts, font_size, spacing);
        runs.push_back(InlineRun{ run.text, run_width, run.bold, run.italic, run.strikethrough, run.code, run.kind });
        width += run_width;
    }
}

std::vector<TextLine> wrap_words_to_lines(const std::vector<Word> &words, const FontSet &fonts, float font_size, float max_width) {
    std::vector<TextLine> lines;

    if (words.empty()) {
        lines.push_back(TextLine{ {}, 0.0f });
        return lines;
    }

    float spacing     = text_spacing(font_size);
    float space_width = MeasureTextEx(fonts.regular, " ", font_size, spacing).x;

    std::vector<InlineRun> current_runs;
    float current_width = 0.0f;

    for (const Word &word : words) {
        float word_width = 0.0f;
        for (const WordRun &run : word) {
            word_width += measure_run(run, fonts, font_size, spacing);
        }

        float extra = current_runs.empty() ? 0.0f : space_width;

        if (current_width + extra + word_width <= max_width || current_runs.empty()) {
            if (!current_runs.empty()) {
                current_runs.push_back(InlineRun{ " ", space_width, false, false, false, false });
                current_width += space_width;
            }
            append_word(current_runs, current_width, word, fonts, font_size, spacing);
        } else {
            lines.push_back(TextLine{ std::move(current_runs), current_width });
            current_runs.clear();
            current_width = 0.0f;
            append_word(current_runs, current_width, word, fonts, font_size, spacing);
        }
    }

    if (!current_runs.empty()) {
        lines.push_back(TextLine{ std::move(current_runs), current_width });
    }

    return lines;
}

// "Infinite" width used to measure a table cell as a single line, without
// wrapping — a table doesn't reflow (see BlockKind::Table).
constexpr float kUnboundedTableCellWidth = 1e6f;

// Measures a table cell as a single TextLine (never more than one —
// kUnboundedTableCellWidth guarantees wrap_words_to_lines won't wrap it).
// `cell` nullptr (a body row with fewer cells than columns, see
// ContentBlock::table_rows) becomes an empty TextLine, width 0.
// `force_bold`: true for the header, always bold (see
// layout_at_font_size).
TextLine measure_table_cell(const TableCell *cell, const FontSet &fonts, float font_size, bool force_bold) {
    std::vector<Word> words = cell != nullptr ? tokenize_words(cell->spans, force_bold, false) : std::vector<Word>{};
    return wrap_words_to_lines(words, fonts, font_size, kUnboundedTableCellWidth)[0];
}

// Joins every span's raw text into one string, stripping a single
// trailing '\n' if present — fenced code blocks (and mermaid diagrams,
// which share the same underlying md4c code-block parsing) always end
// their last line with '\n'; without stripping it we'd end up with an
// extra blank line (layout_code_lines) or a trailing newline in the
// diagram source handed to nixie (see ensure_mermaid_image_loaded).
std::string join_span_text(const std::vector<TextSpan> &spans) {
    std::string raw;
    for (const TextSpan &span : spans) {
        raw += span.text;
    }
    if (!raw.empty() && raw.back() == '\n') {
        raw.pop_back();
    }
    return raw;
}

// Code block: each line of the original text (split by '\n') becomes its
// own TextLine, with content preserved literally (indentation and internal
// spaces included) — no reflow. If a line is too wide, it overflows
// horizontally (accepted; contributes to block_width and triggers the same
// font-shrink as the rest of the layout).
std::vector<TextLine> layout_code_lines(const std::vector<TextSpan> &spans, const Font &mono_font, float font_size) {
    std::string raw = join_span_text(spans);

    float spacing = text_spacing(font_size);
    std::vector<TextLine> lines;
    std::string current;

    auto flush_line = [&]() {
        TextLine line;
        if (!current.empty()) {
            float width = MeasureTextEx(mono_font, current.c_str(), font_size, spacing).x;
            line.runs.push_back(InlineRun{ current, width, false, false, false, true });
            line.width = width;
        } else {
            line.width = 0.0f;
        }
        lines.push_back(std::move(line));
        current.clear();
    };

    for (char c : raw) {
        if (c == '\n') {
            flush_line();
        } else {
            current.push_back(c);
        }
    }
    flush_line();

    return lines;
}

// Sizes an already-loaded image (or a successfully-rendered mermaid
// diagram, see the is_mermaid branch below) to the largest scale, keeping
// its aspect ratio, that fits within kImageMaxWidthFraction of max_width
// and kImageMaxHeightFraction of available_height — never upscaled beyond
// its native size (see the field comments on LayoutBlock::image_width/
// image_height in text_layout.h). Pushes the single empty "line" the
// generic block width/height accumulation below expects.
void fit_image_block(LayoutBlock &layout_block, float natural_width, float natural_height, float max_width, float available_height) {
    float scale_to_fit        = std::min({ max_width * kImageMaxWidthFraction / natural_width, available_height * kImageMaxHeightFraction / natural_height, 1.0f });
    layout_block.image_width  = natural_width * scale_to_fit;
    layout_block.image_height = natural_height * scale_to_fit;
    layout_block.line_height  = layout_block.image_height;

    TextLine image_line;
    image_line.width = layout_block.image_width;
    layout_block.lines.push_back(std::move(image_line));
}

TextLayoutResult layout_at_font_size(const std::vector<ContentBlock> &content, const FontSet &fonts, const AppConfig &config, float base_font_size, float max_width, float available_height, ImageCache &image_cache, const std::string &mermaid_font_path, std::optional<ThemeKind> theme) {
    TextLayoutResult result;
    result.block_width  = 0.0f;
    result.block_height = 0.0f;

    float layout_scale = base_font_size / config.default_font_size;

    for (size_t i = 0; i < content.size(); ++i) {
        const ContentBlock &content_block = content[i];
        bool is_heading                   = content_block.kind == BlockKind::Heading;
        bool is_code_block                = content_block.kind == BlockKind::CodeBlock;
        bool is_thematic_break            = content_block.kind == BlockKind::ThematicBreak;
        bool is_block_quote               = content_block.kind == BlockKind::BlockQuote;
        bool is_list_item                 = content_block.kind == BlockKind::ListItem;
        bool is_image                     = content_block.kind == BlockKind::Image;
        bool is_mermaid                   = content_block.kind == BlockKind::Mermaid;
        bool is_table                     = content_block.kind == BlockKind::Table;
        float scale                       = 1.0f;
        if (is_heading) {
            scale = heading_scale(config, content_block.heading_level);
        } else if (is_block_quote) {
            scale = config.blockquote_scale;
        }
        float font_size = base_font_size * scale;

        LayoutBlock layout_block;
        layout_block.font_size         = font_size;
        layout_block.is_code_block     = is_code_block;
        layout_block.is_thematic_break = is_thematic_break;
        layout_block.is_block_quote    = is_block_quote;
        layout_block.is_list_item      = is_list_item;
        layout_block.is_image          = is_image;
        layout_block.image_texture     = nullptr;
        layout_block.image_width       = 0.0f;
        layout_block.image_height      = 0.0f;
        layout_block.is_table          = is_table;
        layout_block.table_row_height  = 0.0f;

        // The space ABOVE a thematic break is smaller than normal (sits closer
        // to the text above); consecutive items of the same list stay flush
        // (just line_height between them, no extra space) — we look at the
        // next block to decide the space that goes after this one.
        if (i + 1 < content.size()) {
            bool next_is_rule    = content[i + 1].kind == BlockKind::ThematicBreak;
            bool both_list_items = is_list_item && content[i + 1].kind == BlockKind::ListItem;
            float spacing;
            if (next_is_rule) {
                spacing = config.horizontal_rule_top_spacing;
            } else if (both_list_items) {
                spacing = 0.0f;
            } else {
                spacing = config.paragraph_spacing;
            }
            layout_block.spacing_after = spacing * layout_scale;
        } else {
            layout_block.spacing_after = 0.0f;
        }

        float block_extra_width  = 0.0f;
        float block_extra_height = 0.0f;

        if (is_thematic_break) {
            layout_block.line_height = font_size * kThematicBreakThicknessFraction;
            TextLine rule_line;
            rule_line.width = max_width;
            layout_block.lines.push_back(std::move(rule_line));
        } else if (is_code_block) {
            layout_block.line_height = font_size * config.line_height_multiplier;
            layout_block.lines       = layout_code_lines(content_block.spans, fonts.mono, font_size);
            float padding            = font_size * kCodeBlockPaddingFraction;
            block_extra_width        = 2.0f * padding;
            block_extra_height       = 2.0f * padding;
        } else if (is_list_item) {
            layout_block.line_height           = font_size * config.line_height_multiplier;
            layout_block.list_marker           = list_marker_text(content_block);
            layout_block.list_marker_font_size = font_size * list_marker_font_scale(content_block);
            layout_block.list_indent           = font_size * kListIndentPerLevelFraction * static_cast<float>(content_block.list_level);

            float marker_spacing = text_spacing(layout_block.list_marker_font_size);
            float marker_width   = MeasureTextEx(fonts.regular, layout_block.list_marker.c_str(), layout_block.list_marker_font_size, marker_spacing).x;
            float text_indent    = layout_block.list_indent + marker_width + font_size * kListMarkerGapFraction;

            std::vector<Word> words = tokenize_words(content_block.spans, false, false);
            layout_block.lines      = wrap_words_to_lines(words, fonts, font_size, max_width - text_indent);
            block_extra_width       = text_indent;
        } else if (is_image) {
            const Texture2D *texture   = ensure_image_loaded(image_cache, content_block.image_path);
            layout_block.image_texture = texture;

            if (texture != nullptr && texture->width > 0 && texture->height > 0) {
                // Image loaded: reuses the same block width/height accumulation
                // that any other block type already uses right below
                // (block_max_line_width / lines.size() * line_height). What
                // actually draws it is render/text_renderer.cpp, using
                // image_texture/image_width/image_height.
                fit_image_block(layout_block, static_cast<float>(texture->width), static_cast<float>(texture->height), max_width, available_height);
            } else {
                // Image not found/not loaded: falls back to the alt text, treated
                // as a normal paragraph (same reflow, same font) — the text
                // fallback documented in markdown/markdown_parser.h.
                layout_block.line_height = font_size * config.line_height_multiplier;
                std::vector<Word> words  = tokenize_words(content_block.spans, false, false);
                layout_block.lines       = wrap_words_to_lines(words, fonts, font_size, max_width);
            }
        } else if (is_mermaid) {
            std::string mermaid_source = join_span_text(content_block.spans);
            const Texture2D *texture   = ensure_mermaid_image_loaded(image_cache, mermaid_source, mermaid_font_path, theme);

            if (texture != nullptr && texture->width > 0 && texture->height > 0) {
                // Rendered successfully: from here on, treated exactly like a
                // loaded BlockKind::Image (same drawing path in
                // render/text_renderer.cpp) — is_image is set here, not at the
                // top of the loop, since content_block.kind is Mermaid, not
                // Image.
                layout_block.is_image      = true;
                layout_block.image_texture = texture;
                fit_image_block(layout_block, static_cast<float>(texture->width), static_cast<float>(texture->height), max_width, available_height);
            } else {
                // nixie couldn't parse/render the diagram: falls back to
                // showing its raw source, same treatment as a plain
                // BlockKind::CodeBlock — a visible signal that something's
                // off, without a dedicated error UI (mirrors BlockKind::Image
                // falling back to its alt text above).
                layout_block.is_code_block = true;
                layout_block.line_height   = font_size * config.line_height_multiplier;
                layout_block.lines         = layout_code_lines(content_block.spans, fonts.mono, font_size);
                float padding              = font_size * kCodeBlockPaddingFraction;
                block_extra_width          = 2.0f * padding;
                block_extra_height         = 2.0f * padding;
            }
        } else if (is_table) {
            float text_line_height          = font_size * config.line_height_multiplier;
            layout_block.table_column_align = content_block.table_column_align;
            layout_block.table_row_height   = text_line_height + 2.0f * font_size * kTableCellPaddingYFraction;

            size_t column_count  = content_block.table_column_align.size();
            float cell_padding_x = font_size * kTableCellPaddingXFraction;
            layout_block.table_column_width.assign(column_count, 0.0f);

            auto measure_row = [&](const TableRow &row, bool force_bold) {
                std::vector<TextLine> cells;
                cells.reserve(column_count);
                for (size_t col = 0; col < column_count; ++col) {
                    const TableCell *cell                = col < row.cells.size() ? &row.cells[col] : nullptr;
                    TextLine line                        = measure_table_cell(cell, fonts, font_size, force_bold);
                    layout_block.table_column_width[col] = std::max(layout_block.table_column_width[col], line.width + 2.0f * cell_padding_x);
                    cells.push_back(std::move(line));
                }
                return cells;
            };

            if (!content_block.table_rows.empty()) {
                layout_block.table_header_row = measure_row(content_block.table_rows[0], /*force_bold=*/true);
            }
            for (size_t r = 1; r < content_block.table_rows.size(); ++r) {
                layout_block.table_body_rows.push_back(measure_row(content_block.table_rows[r], /*force_bold=*/false));
            }

            float table_width = 0.0f;
            for (float w : layout_block.table_column_width) {
                table_width += w;
            }

            // `lines` doesn't carry the table's text (that's in
            // table_header_row/table_body_rows) — just one empty TextLine per
            // table row (header + body), so the generic block width/height
            // computation right below can reuse the same formula the other
            // block types already use.
            layout_block.line_height = layout_block.table_row_height;
            size_t total_rows        = 1 + layout_block.table_body_rows.size();
            for (size_t r = 0; r < total_rows; ++r) {
                layout_block.lines.push_back(TextLine{ {}, table_width });
            }
        } else {
            layout_block.line_height = font_size * config.line_height_multiplier;
            float quote_indent       = is_block_quote ? font_size * (kBlockQuoteBarWidthFraction + kBlockQuoteGapFraction) : 0.0f;
            std::vector<Word> words  = tokenize_words(content_block.spans, is_heading, is_block_quote);
            layout_block.lines       = wrap_words_to_lines(words, fonts, font_size, max_width - quote_indent);
            block_extra_width        = quote_indent;
        }

        float block_max_line_width = 0.0f;
        for (const TextLine &line : layout_block.lines) {
            block_max_line_width = std::max(block_max_line_width, line.width);
        }

        if (is_list_item) {
            layout_block.list_box_width = block_max_line_width + block_extra_width;
        }

        result.block_width = std::max(result.block_width, block_max_line_width + block_extra_width);
        result.block_height += static_cast<float>(layout_block.lines.size()) * layout_block.line_height + block_extra_height;
        result.block_height += layout_block.spacing_after;

        result.blocks.push_back(std::move(layout_block));
    }

    // A list is not a single block: each item is its own LayoutBlock, and by
    // default each block is aligned (centered, or left/right per the slide's
    // "align" parameter) by its own width — which would leave the items'
    // markers out of column. For a list's items to line up as a single unit
    // (and, if the slide has more than one list, for the lists to line up
    // with each other — the "Unordered" and "Ordered" ones on the same
    // slide, for example), every list item on the slide ends up sharing the
    // largest list_box_width among all of them, regardless of whether they
    // sit in different contiguous runs.
    float list_group_width = 0.0f;
    for (const LayoutBlock &block : result.blocks) {
        if (block.is_list_item) {
            list_group_width = std::max(list_group_width, block.list_box_width);
        }
    }
    for (LayoutBlock &block : result.blocks) {
        if (block.is_list_item) {
            block.list_box_width = list_group_width;
        }
    }

    return result;
}

} // namespace

const Font &select_styled_font(bool bold, bool italic, bool code, GlyphFontKind kind, const FontSet &fonts) {
    if (code) {
        return fonts.mono;
    }
    if (kind == GlyphFontKind::Emoji) {
        return fonts.emoji;
    }
    if (kind == GlyphFontKind::Asian) {
        return fonts.asian;
    }
    if (bold && italic) {
        return fonts.bold_italic;
    }
    if (bold) {
        return fonts.bold;
    }
    if (italic) {
        return fonts.italic;
    }
    return fonts.regular;
}

TextLayoutResult compute_fitted_layout(const std::vector<ContentBlock> &content, const FontSet &fonts, const AppConfig &config, float initial_font_size, float available_width, float available_height, ImageCache &image_cache, const std::string &mermaid_font_path, std::optional<ThemeKind> theme) {
    float font_size = std::clamp(initial_font_size, config.min_font_size, config.max_font_size);

    TextLayoutResult result = layout_at_font_size(content, fonts, config, font_size, available_width, available_height, image_cache, mermaid_font_path, theme);

    while (font_size > config.min_font_size && (result.block_height > available_height || result.block_width > available_width)) {
        font_size = std::max(config.min_font_size, font_size - kFontShrinkStep);
        result    = layout_at_font_size(content, fonts, config, font_size, available_width, available_height, image_cache, mermaid_font_path, theme);
    }

    return result;
}
