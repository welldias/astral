#include "slides/slide_deck.h"

#include "assets/image_cache.h"
#include "slides/slide_splitter.h"

SlideDeck build_slide_deck(const std::string& document_text, const std::string& base_dir) {
  SlideDeck deck{};  // zero-initializes deck.usage — without this the bools hold garbage memory

  for (const SlideSource& slide_source : split_into_slides(document_text)) {
    std::vector<ContentBlock> blocks = parse_markdown(slide_source.text);
    StyleUsage usage = detect_style_usage(blocks);

    deck.usage.bold = deck.usage.bold || usage.bold;
    deck.usage.italic = deck.usage.italic || usage.italic;
    deck.usage.bold_italic = deck.usage.bold_italic || usage.bold_italic;
    deck.usage.code = deck.usage.code || usage.code;

    CodepointUsage codepoint_usage = detect_codepoint_usage(blocks);
    deck.codepoints.emoji_codepoints.insert(codepoint_usage.emoji_codepoints.begin(),
                                             codepoint_usage.emoji_codepoints.end());
    deck.codepoints.asian_codepoints.insert(codepoint_usage.asian_codepoints.begin(),
                                             codepoint_usage.asian_codepoints.end());
    deck.codepoints.other_codepoints.insert(codepoint_usage.other_codepoints.begin(),
                                             codepoint_usage.other_codepoints.end());

    for (ContentBlock& block : blocks) {
      if (block.kind == BlockKind::Image) {
        block.image_path = resolve_image_path(base_dir, block.image_path);
      }
    }

    deck.slides.push_back(Slide{std::move(blocks), slide_source.params});
  }

  return deck;
}
