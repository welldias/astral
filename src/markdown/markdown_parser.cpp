#include "markdown/markdown_parser.h"

#include <md4c.h>
#include <raylib.h>

#include <sstream>

#include "text/glyph_class.h"

namespace {

bool is_blank_line(const std::string &line) {
    return line.find_first_not_of(" \t") == std::string::npos;
}

// Opening/closing line of a fenced code block (``` or ~~~, 3+ characters).
// Used only to avoid touching lines inside a code block while protecting
// thematic breaks (see protect_thematic_breaks) — the content of a code
// block is preserved literally, so inserting a blank line in there would
// alter the block's text.
bool is_code_fence_line(const std::string &line) {
    size_t i = 0;
    while (i < line.size() && line[i] == ' ') {
        ++i;
    }
    if (i >= line.size()) {
        return false;
    }
    char marker = line[i];
    if (marker != '`' && marker != '~') {
        return false;
    }
    size_t count = 0;
    while (i < line.size() && line[i] == marker) {
        ++count;
        ++i;
    }
    return count >= 3;
}

// A line that CommonMark recognizes as a thematic break: only spaces/tabs
// and one repeated character among '-', '_' and '*', 3 or more times (the
// same marker documented in markdown_parser.h for BlockKind::ThematicBreak).
bool is_thematic_break_line(const std::string &line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }
    if (i >= line.size()) {
        return false;
    }
    char marker = line[i];
    if (marker != '-' && marker != '_' && marker != '*') {
        return false;
    }

    size_t count = 0;
    for (; i < line.size(); ++i) {
        char c = line[i];
        if (c == marker) {
            ++count;
        } else if (c != ' ' && c != '\t') {
            return false;
        }
    }
    return count >= 3;
}

// md4c follows CommonMark to the letter: a "---" (or "***"/"___") line
// right below a line of text, with no blank line between them, is the
// underline of a "setext heading" — the line above becomes a heading and
// the "---" is consumed along with it, never surviving as its own block.
// This holds even when the intent is this marker's dedicated thematic
// break (see BlockKind::ThematicBreak). For the marker to work in any
// context, we guarantee a blank line before it whenever it's stuck right
// below text — except inside a code block, where the line is literal
// content and must not be touched.
std::string protect_thematic_breaks(const std::string &source) {
    std::vector<std::string> lines;
    {
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }

    std::string result;
    bool inside_code_fence = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        if (is_code_fence_line(lines[i])) {
            inside_code_fence = !inside_code_fence;
        } else if (!inside_code_fence && i > 0 && is_thematic_break_line(lines[i]) && !is_blank_line(lines[i - 1])) {
            result += '\n';
        }

        if (i > 0) {
            result += '\n';
        }
        result += lines[i];
    }

    return result;
}

// An open list (UL/OL): the type applies to all of its direct items, and
// next_index numbers those items (only used if ordered; starts at
// MD_BLOCK_OL_DETAIL::start, normally 1). Pushed onto
// ParserContext::list_stack — the top is the currently innermost list.
struct OpenList {
    bool ordered;
    int next_index;
};

struct ParserContext {
    std::vector<ContentBlock> *blocks;
    int bold_depth   = 0;
    int italic_depth = 0;
    int strike_depth = 0;
    int code_depth   = 0;
    // >0 while inside a blockquote (BLOCK_QUOTE), including nested ones
    // ("> > text"). Only the first level (0 -> 1) opens the ContentBlock;
    // everything that comes after — paragraphs, nested blockquotes, lists...
    // — falls into that same already-open block, see the guard right at the
    // start of enter_block_callback.
    int quote_depth = 0;
    // Stack of open lists (UL/OL) — its index is the nesting level
    // (ContentBlock::list_level) of the next list item.
    std::vector<OpenList> list_stack;
    // >0 while inside a list item (LI), including nested ones. Each LI opens
    // its own ContentBlock (unlike blockquote); this only controls absorbing
    // the inner paragraph that md4c emits inside each LI, so as not to
    // duplicate the block — see the guard further below.
    int list_item_depth = 0;

    // >0 while inside an image (MD_SPAN_IMG) — includes an image nested in
    // another image's own alt text (a rare md4c case; only the outermost one
    // is considered, see enter_span_callback). An image's alt text arrives
    // via text_callback as regular text (which is why it always ends up in
    // ContentBlock::spans, serving as the fallback); this counter only
    // distinguishes text THAT CAME from inside an image (doesn't invalidate
    // the "paragraph is just a lone image" hypothesis below) from text
    // outside it (does invalidate it).
    int image_depth = 0;
    // Track whether the paragraph (Paragraph) currently being read can turn
    // into BlockKind::Image in leave_block_callback: only when it contains
    // exactly ONE image and nothing else (no text outside it). Reset whenever
    // a real new Paragraph is opened (see enter_block_callback); invalidated
    // by any text outside an image (text_callback) or by a second image
    // (enter_span_callback).
    bool paragraph_is_bare_image = false;
    int paragraph_image_count    = 0;
    std::string paragraph_image_path;

    // >0 while inside a table (TABLE), including TR/TH/TD — like
    // blockquote/list item, it absorbs any nested block with no dedicated
    // handling (though TR/TH/TD already have their own, see the guard
    // further below). Tables don't nest (the GFM grammar doesn't allow it),
    // so no quote_depth-style counter is needed: a TABLE is only opened when
    // table_depth==0.
    int table_depth    = 0;
    bool in_table_cell = false; // inside TH/TD — text goes to current_table_cell_spans
    std::vector<TableCell> current_table_row;
    std::vector<TextSpan> current_table_cell_spans;
};

int enter_block_callback(MD_BLOCKTYPE type, void *detail, void *userdata) {
    auto *ctx = static_cast<ParserContext *>(userdata);

    if (type == MD_BLOCK_DOC) {
        return 0;
    }

    if (type == MD_BLOCK_QUOTE) {
        if (ctx->quote_depth == 0) {
            ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::BlockQuote });
        }
        ++ctx->quote_depth;
        return 0;
    }

    if (ctx->quote_depth > 0) {
        // Already inside a blockquote: all content falls into the blockquote
        // ContentBlock already opened above, without opening a new block (see
        // ParserContext::quote_depth) — the same "flatten into a single block"
        // simplification applied here to the blockquote (including a
        // list/table inside it, which outside of here have their own
        // dedicated handling).
        return 0;
    }

    if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
        bool ordered    = type == MD_BLOCK_OL;
        int start_index = 1;
        if (ordered) {
            start_index = static_cast<int>(static_cast<MD_BLOCK_OL_DETAIL *>(detail)->start);
        }
        ctx->list_stack.push_back(OpenList{ ordered, start_index });
        return 0;
    }

    if (type == MD_BLOCK_LI) {
        if (!ctx->list_stack.empty()) {
            OpenList &list = ctx->list_stack.back();
            int level      = static_cast<int>(ctx->list_stack.size()) - 1;
            int index      = list.next_index++;
            ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::ListItem, .list_level = level, .list_index = index, .list_ordered = list.ordered });
        }
        ++ctx->list_item_depth;
        return 0;
    }

    if (ctx->list_item_depth > 0) {
        // Already inside a list item: the paragraph that md4c emits for the
        // item's text (and any other block with no dedicated handling nested
        // inside it) falls into the item's ContentBlock already opened above,
        // without opening a new block — see ParserContext::list_item_depth.
        return 0;
    }

    if (type == MD_BLOCK_TABLE) {
        auto *table_detail = static_cast<MD_BLOCK_TABLE_DETAIL *>(detail);
        ContentBlock table_block{ .kind = BlockKind::Table };
        table_block.table_column_align.assign(table_detail->col_count, TextAlign::Left);
        ctx->blocks->push_back(std::move(table_block));
        ++ctx->table_depth;
        return 0;
    }

    if (ctx->table_depth > 0) {
        if (type == MD_BLOCK_TR) {
            ctx->current_table_row.clear();
            return 0;
        }
        if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
            ctx->in_table_cell = true;
            ctx->current_table_cell_spans.clear();

            // Column alignment: comes from MD_BLOCK_TD_DETAIL on every cell
            // (header and body) — md4c already resolves it from ":" in the
            // delimiter line (see ContentBlock::table_column_align), so it's
            // enough to apply it from any one of them.
            auto *td_detail = static_cast<MD_BLOCK_TD_DETAIL *>(detail);
            TextAlign align = TextAlign::Left; // covers MD_ALIGN_DEFAULT and MD_ALIGN_LEFT alike
            if (td_detail->align == MD_ALIGN_CENTER) {
                align = TextAlign::Center;
            } else if (td_detail->align == MD_ALIGN_RIGHT) {
                align = TextAlign::Right;
            }
            size_t column       = ctx->current_table_row.size();
            ContentBlock &table = ctx->blocks->back();
            if (column < table.table_column_align.size()) {
                table.table_column_align[column] = align;
            }
            return 0;
        }
        // THEAD/TBODY: containers with no content of their own, nothing to do
        // besides letting the cells inside them fall into the cases above.
        return 0;
    }

    if (type == MD_BLOCK_H) {
        auto *h_detail = static_cast<MD_BLOCK_H_DETAIL *>(detail);
        int level      = static_cast<int>(h_detail->level);
        if (level > 4) {
            level = 4;
        }
        ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::Heading, .heading_level = level });
        return 0;
    }

    if (type == MD_BLOCK_CODE) {
        ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::CodeBlock });
        return 0;
    }

    if (type == MD_BLOCK_HR) {
        ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::ThematicBreak });
        return 0;
    }

    // Any other block type with no dedicated handling yet falls back to a
    // plain paragraph, so as not to lose the text. Resets the tracking of
    // "paragraph is just a lone image" (see
    // ParserContext::paragraph_is_bare_image) — MD_BLOCK_P always goes
    // through here when it isn't being absorbed by a blockquote/list item
    // (the "return 0"s above already covered those cases and the table one),
    // so every Paragraph pushed at this point is, in fact, a genuinely new
    // block.
    ctx->blocks->push_back(ContentBlock{ .kind = BlockKind::Paragraph });
    ctx->paragraph_is_bare_image = true;
    ctx->paragraph_image_count   = 0;
    ctx->paragraph_image_path.clear();
    return 0;
}

int leave_block_callback(MD_BLOCKTYPE type, void * /*detail*/, void *userdata) {
    auto *ctx = static_cast<ParserContext *>(userdata);
    if (type == MD_BLOCK_QUOTE) {
        --ctx->quote_depth;
    } else if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
        if (!ctx->list_stack.empty()) {
            ctx->list_stack.pop_back();
        }
    } else if (type == MD_BLOCK_LI) {
        --ctx->list_item_depth;
    } else if (type == MD_BLOCK_TABLE) {
        --ctx->table_depth;
    } else if (ctx->table_depth > 0 && type == MD_BLOCK_TR) {
        ctx->blocks->back().table_rows.push_back(TableRow{ std::move(ctx->current_table_row) });
        ctx->current_table_row.clear();
    } else if (ctx->table_depth > 0 && (type == MD_BLOCK_TH || type == MD_BLOCK_TD)) {
        ctx->current_table_row.push_back(TableCell{ std::move(ctx->current_table_cell_spans) });
        ctx->current_table_cell_spans.clear();
        ctx->in_table_cell = false;
    } else if (type == MD_BLOCK_P && ctx->paragraph_is_bare_image && ctx->paragraph_image_count == 1 && !ctx->blocks->empty()) {
        // block.kind is only Paragraph here if this MD_BLOCK_P actually pushed
        // a new block (see the reset in enter_block_callback) — a paragraph
        // absorbed by a blockquote/list item leaves blocks->back() as
        // BlockQuote/ListItem, not Paragraph, and this guard leaves it alone.
        ContentBlock &block = ctx->blocks->back();
        if (block.kind == BlockKind::Paragraph) {
            block.kind       = BlockKind::Image;
            block.image_path = ctx->paragraph_image_path;
            // block.spans already has the alt text (accumulated normally via
            // text_callback while the image was open) — it becomes the textual
            // fallback if the image fails to load (see text/text_layout.h).
        }
    }
    return 0;
}

int enter_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
    auto *ctx = static_cast<ParserContext *>(userdata);
    switch (type) {
    case MD_SPAN_STRONG:
        ++ctx->bold_depth;
        break;
    case MD_SPAN_EM:
        ++ctx->italic_depth;
        break;
    case MD_SPAN_DEL:
        ++ctx->strike_depth;
        break;
    case MD_SPAN_CODE:
        ++ctx->code_depth;
        break;
    case MD_SPAN_IMG: {
        ++ctx->paragraph_image_count;
        if (ctx->image_depth == 0) {
            // Only the outermost image counts — md4c (rarely) allows an image
            // nested in another one's alt text; we ignore the inner one.
            auto *img_detail = static_cast<MD_SPAN_IMG_DETAIL *>(detail);
            ctx->paragraph_image_path.assign(img_detail->src.text, img_detail->src.size);
            // The comment (MD_SPAN_IMG_DETAIL::title) is ignored on purpose:
            // unused by Astral (see markdown_parser.h).
        }
        ++ctx->image_depth;
        break;
    }
    default:
        break;
    }
    return 0;
}

int leave_span_callback(MD_SPANTYPE type, void * /*detail*/, void *userdata) {
    auto *ctx = static_cast<ParserContext *>(userdata);
    switch (type) {
    case MD_SPAN_STRONG:
        --ctx->bold_depth;
        break;
    case MD_SPAN_EM:
        --ctx->italic_depth;
        break;
    case MD_SPAN_DEL:
        --ctx->strike_depth;
        break;
    case MD_SPAN_CODE:
        --ctx->code_depth;
        break;
    case MD_SPAN_IMG:
        --ctx->image_depth;
        break;
    default:
        break;
    }
    return 0;
}

void append_text(ParserContext *ctx, const std::string &text) {
    if (ctx->in_table_cell) {
        // Text from inside a cell (TH/TD) goes to the buffer of the cell being
        // read, not to the table's ContentBlock::spans (which stays empty —
        // see ContentBlock::table_rows).
        ctx->current_table_cell_spans.push_back(TextSpan{ text, ctx->bold_depth > 0, ctx->italic_depth > 0, ctx->strike_depth > 0, ctx->code_depth > 0 });
        return;
    }
    if (ctx->blocks->empty()) {
        return;
    }
    ctx->blocks->back().spans.push_back(TextSpan{ text, ctx->bold_depth > 0, ctx->italic_depth > 0, ctx->strike_depth > 0, ctx->code_depth > 0 });
}

int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    auto *ctx = static_cast<ParserContext *>(userdata);

    if (ctx->image_depth == 0) {
        // Text outside any image: if this happens inside the paragraph an
        // image also occupies, it's no longer "alone" — invalidates the
        // bare-image hypothesis (see ParserContext::paragraph_is_bare_image).
        // Text that comes from INSIDE the image (the alt text) invalidates
        // nothing; it still falls into spans normally right below, serving as
        // the fallback.
        ctx->paragraph_is_bare_image = false;
    }

    switch (type) {
    case MD_TEXT_BR:
    case MD_TEXT_SOFTBR:
        // Soft line break inside the block: just a space, the wrap (done by
        // us) decides where the line actually breaks.
        append_text(ctx, " ");
        break;
    case MD_TEXT_NULLCHAR:
        append_text(ctx, "\xEF\xBF\xBD"); // U+FFFD REPLACEMENT CHARACTER
        break;
    default:
        append_text(ctx, std::string(text, size));
        break;
    }

    return 0;
}

} // namespace

std::vector<ContentBlock> parse_markdown(const std::string &source) {
    std::string protected_source = protect_thematic_breaks(source);

    std::vector<ContentBlock> blocks;
    ParserContext ctx;
    ctx.blocks = &blocks;

    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags       = MD_FLAG_STRIKETHROUGH | MD_FLAG_TABLES;
    parser.enter_block = enter_block_callback;
    parser.leave_block = leave_block_callback;
    parser.enter_span  = enter_span_callback;
    parser.leave_span  = leave_span_callback;
    parser.text        = text_callback;
    parser.debug_log   = nullptr;
    parser.syntax      = nullptr;

    md_parse(protected_source.data(), static_cast<MD_SIZE>(protected_source.size()), &parser, &ctx);

    return blocks;
}

StyleUsage detect_style_usage(const std::vector<ContentBlock> &content) {
    StyleUsage usage{ false, false, false, false };

    for (const ContentBlock &block : content) {
        if (block.kind == BlockKind::CodeBlock) {
            usage.code = true;
            continue;
        }

        if (block.kind == BlockKind::Table) {
            // A table's text lives in table_rows, not in spans (see
            // ContentBlock) — scan it separately. The header (row 0, guaranteed
            // by the GFM grammar) is always drawn in bold (see
            // text/text_layout.h), so the bold font needs to be loaded even if
            // no cell explicitly uses **bold**.
            usage.bold = true;
            for (size_t row_index = 0; row_index < block.table_rows.size(); ++row_index) {
                bool force_bold = row_index == 0;
                for (const TableCell &cell : block.table_rows[row_index].cells) {
                    for (const TextSpan &span : cell.spans) {
                        if (span.code) {
                            usage.code = true;
                            continue;
                        }
                        bool bold         = span.bold || force_bold;
                        usage.bold        = usage.bold || bold;
                        usage.italic      = usage.italic || span.italic;
                        usage.bold_italic = usage.bold_italic || (bold && span.italic);
                    }
                }
            }
            continue;
        }

        bool block_bold   = block.kind == BlockKind::Heading;    // heading forces bold
        bool block_italic = block.kind == BlockKind::BlockQuote; // blockquote forces italic
        for (const TextSpan &span : block.spans) {
            if (span.code) {
                usage.code = true;
                continue; // code always uses the mono font, doesn't count toward bold/italic
            }

            bool bold         = span.bold || block_bold;
            bool italic       = span.italic || block_italic;
            usage.bold        = usage.bold || bold;
            usage.italic      = usage.italic || italic;
            usage.bold_italic = usage.bold_italic || (bold && italic);
        }
    }

    return usage;
}

namespace {

void collect_span_codepoints(const std::string &text, CodepointUsage &usage) {
    for (const GlyphRun &run : split_by_glyph_kind(text)) {
        if (run.kind == GlyphFontKind::Base) {
            continue;
        }
        int codepoint_size = 0;
        for (size_t i = 0; i < run.text.size(); i += static_cast<size_t>(codepoint_size)) {
            int codepoint = GetCodepointNext(run.text.c_str() + i, &codepoint_size);
            if (codepoint_size <= 0) {
                codepoint_size = 1;
            }
            switch (run.kind) {
            case GlyphFontKind::Emoji:
                usage.emoji_codepoints.insert(codepoint);
                break;
            case GlyphFontKind::Asian:
                usage.asian_codepoints.insert(codepoint);
                break;
            case GlyphFontKind::Other:
                usage.other_codepoints.insert(codepoint);
                break;
            case GlyphFontKind::Base:
                break;
            }
        }
    }
}

void collect_block_codepoints(const ContentBlock &block, CodepointUsage &usage) {
    for (const TextSpan &span : block.spans) {
        collect_span_codepoints(span.text, usage);
    }
    for (const TableRow &row : block.table_rows) {
        for (const TableCell &cell : row.cells) {
            for (const TextSpan &span : cell.spans) {
                collect_span_codepoints(span.text, usage);
            }
        }
    }
}

} // namespace

CodepointUsage detect_codepoint_usage(const std::vector<ContentBlock> &content) {
    CodepointUsage usage;
    for (const ContentBlock &block : content) {
        collect_block_codepoints(block, usage);
    }
    return usage;
}
