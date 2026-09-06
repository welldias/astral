#pragma once

#include <string>
#include <vector>

#include "markdown/markdown_parser.h"
#include "slides/slide_splitter.h"

// A slide already parsed as Markdown, plus the layout parameters read from
// the marker that precedes it (see slides/slide_splitter.h::SlideParams).
struct Slide {
    std::vector<ContentBlock> content;
    SlideParams params;
};

// All the slides of a document, already split (see
// slides/slide_splitter.h) and parsed as Markdown, plus the union of the
// styles used by any of them — to load the needed fonts all at once,
// without a stutter when navigating between slides (see
// render/text_renderer.h::ensure_styles_loaded). codepoints is the same
// idea for the emoji/Asian/other characters found in any slide (see
// render/text_renderer.h::ensure_extra_fonts_loaded).
struct SlideDeck {
    std::vector<Slide> slides;
    StyleUsage usage;
    CodepointUsage codepoints;
};

// `base_dir` is the source .md file's directory (see
// assets/image_cache.h::resolve_image_path) — used to resolve, into an
// absolute path, the `image_path` of every ContentBlock::kind == Image
// found in the slides.
SlideDeck build_slide_deck(const std::string &document_text, const std::string &base_dir);
