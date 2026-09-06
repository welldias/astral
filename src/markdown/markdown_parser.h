#pragma once

#include <set>
#include <string>
#include <vector>

#include "text/text_align.h"

enum class BlockKind {
    Paragraph,
    Heading,
    CodeBlock,     // ```...``` (or indented block); verbatim text, no inline style parsing
    ThematicBreak, // line starting with "---" (or "***"/"___"); horizontal rule, no text
    BlockQuote,    // line(s) starting with "> "; forced italic, vertical bar on the left
    ListItem,      // list item ("* text"/"1. text"), possibly nested
    Image,         // "![alt](path)" alone in the paragraph; alt becomes the textual fallback (see spans)
    Table,         // GFM table ("| a | b |\n|---|---|\n| 1 | 2 |"); see ContentBlock::table_rows
};

struct TextSpan {
    std::string text;
    bool bold;
    bool italic;
    bool strikethrough;
    bool code; // inline code (`text`)
};

// A table cell (BlockKind::Table): rich text just like any other Markdown
// span, but with no line wrapping of its own — a table doesn't reflow,
// each cell occupies a single line (see
// text/text_layout.h).
struct TableCell {
    std::vector<TextSpan> spans;
};

struct TableRow {
    std::vector<TableCell> cells;
};

struct ContentBlock {
    BlockKind kind;
    int heading_level; // 1-4, valid only when kind == Heading (levels >4 are clamped to 4)

    // Valid only when kind == ListItem. list_level: nesting depth, 0 = outermost
    // level. list_ordered: true = ordered list ("1.", "2.", ...), false =
    // unordered ("*"/"-"/"+"). list_index: position of the item within the
    // closest list that contains it (resets to 1 for every new list, including
    // nested ones) — only meaningful when list_ordered == true, but it's
    // filled in (1-based) in both cases.
    int list_level;
    int list_index;
    bool list_ordered;

    // Valid only when kind == Image: the image path exactly as written in the
    // Markdown (see slides/slide_deck.cpp for resolving it to an absolute
    // path, relative to the .md file). The alt text ("![alt](...)") has no
    // dedicated field — it comes through in `spans`, like a regular
    // paragraph, to serve as the textual fallback if the image isn't found
    // (see text/text_layout.h).
    std::string image_path;

    // Valid only when kind == Table. table_rows[0] is always the header (the
    // GFM grammar guarantees a table always starts with a header line
    // followed by the alignment delimiter line — the delimiter line itself
    // doesn't become a row, it only defines table_column_align);
    // table_rows[i] for i>=1 are the data rows. table_column_align has one
    // entry per column, in the order declared by the delimiter line
    // ("|---|:---:|---:|"), read once and valid for the whole table.
    // TextAlign::Left also covers the case with no ":" at all (alignment not
    // specified — see BlockKind::Table).
    std::vector<TableRow> table_rows;
    std::vector<TextAlign> table_column_align;

    std::vector<TextSpan> spans; // text with Markdown markup already stripped, inline styles resolved
};

// Parses `source` as Markdown (via md4c) and returns the sequence of blocks
// found. At this stage: heading (#), paragraph, code block (``` ```),
// thematic break (---), blockquote (> text), ordered and unordered lists
// (nestable), image ("![alt](path)"), GFM table (with per-column alignment
// via ":" in the delimiter line — see ContentBlock::table_column_align),
// and the inline styles bold (**text**), italic (*text*), bold-italic
// (***text***), strikethrough (~~text~~) and code (`text`) all get
// dedicated handling; any other block type without dedicated handling yet
// is reduced to a paragraph block with its text, so as not to lose content.
//
// "![alt](path \"comment\")" only becomes BlockKind::Image when it's the
// sole content of the paragraph (alone on the line, with no other text
// before or after it, and no more than one image) — the comment is ignored
// (unused by Astral) and the alt always goes into ContentBlock::spans,
// serving as both the default content AND the textual fallback if the
// image isn't found at runtime (see text/text_layout.h). An image mixed
// with other text in the same paragraph (e.g. "see: ![alt](x.png)") has no
// dedicated handling: the paragraph stays BlockKind::Paragraph and shows
// only the alt as plain text, without attempting to load the image.
//
// A blockquote (BlockKind::BlockQuote) becomes a single ContentBlock even
// if the source Markdown has several paragraphs inside it (or is nested,
// "> > text") — all the text is reduced to one block, without preserving
// the internal structure.
//
// Each list item (BlockKind::ListItem) becomes its own ContentBlock, in
// the order they appear in the document — including items from nested
// lists, identified by ContentBlock::list_level (the list itself,
// "<ul>"/"<ol>", doesn't generate its own block, it only groups the
// items). Combining blockquote, list, or table with each other (one nested
// inside another) has no dedicated handling and may lose structure — each
// type was designed for the common case, on its own.
//
// The thematic break (---, ***, ___) always becomes ThematicBreak, even
// when it's right below a line of text with no blank line between them —
// in that case plain CommonMark would interpret "---" as the underline of
// a "setext heading" (the line above would become a heading instead of a
// paragraph, and the "---" wouldn't even survive as a block); parse_markdown
// inserts a blank line before it to avoid this ambiguity (except inside
// code blocks, where the line is literal content).
std::vector<ContentBlock> parse_markdown(const std::string &source);

struct StyleUsage {
    bool bold;
    bool italic;
    bool bold_italic; // bold AND italic in the same span (needs the dedicated font)
    bool code;
};

// Scans `content` and indicates which font variants are actually used — so
// only what's needed gets loaded (see render/text_renderer.h::ensure_styles_loaded).
// Spans marked as code don't count toward bold/italic: code always uses the
// monospace font, regardless of those styles.
StyleUsage detect_style_usage(const std::vector<ContentBlock> &content);

// Unicode codepoints (deduplicated) found in `content` that aren't covered
// by the standard codepoint set (see text/glyph_class.h::base_codepoints),
// grouped by GlyphFontKind — emoji_codepoints/asian_codepoints feed the
// atlas of the dedicated fonts (--emoji-font/--asian-font, see
// cli/cli_args.h); other_codepoints are from scripts with no dedicated font
// at all (e.g. Cyrillic, Greek, Arabic) — they always fall back to the
// regular font and always generate a warning (see
// render/text_renderer.h::ensure_extra_fonts_loaded).
struct CodepointUsage {
    std::set<int> emoji_codepoints;
    std::set<int> asian_codepoints;
    std::set<int> other_codepoints;
};

CodepointUsage detect_codepoint_usage(const std::vector<ContentBlock> &content);
