#pragma once

#include <raylib.h>

#include <optional>
#include <string>

#include "assets/image_cache.h"
#include "config/theme.h"

// Ensures the Mermaid diagram `mermaid_source` (the raw text of a
// BlockKind::Mermaid block, see markdown/markdown_parser.h) is rendered
// and cached as a Texture2D — idempotent, same scheme as
// assets/image_cache.h::ensure_image_loaded: a source already tried,
// whether successfully or not, is not tried again (see
// ImageCache::mermaid_loaded/mermaid_failed). Callers must treat this as
// cheap to call on every relayout (it's a hash-map lookup after the first
// render) — text/text_layout.cpp's auto-shrink loop calls it repeatedly
// for the same diagram at different font sizes.
//
// `font_path` is forwarded to nixie, which requires a real .ttf/.otf file
// to rasterize the diagram's own text (see nixie_render_png) — pass the
// already-resolved regular font path (see render/text_renderer.h::
// TextRenderer::font_paths). `theme` is Astral's active theme (nullopt if
// no --theme was given, see cli/cli_args.h::CliArgs::theme) — mapped to
// the matching nixie_theme_name_t so the diagram's colors stay legible
// against the slide, whatever the active theme is (see mermaid_render.cpp
// for the mapping, including the two cases with no direct nixie
// equivalent).
//
// Returns nullptr if nixie fails to parse/render the diagram, or if the
// resulting PNG bytes can't be decoded — in that case the caller should
// fall back to showing mermaid_source as plain code text, same as
// BlockKind::CodeBlock (see text/text_layout.cpp).
const Texture2D *ensure_mermaid_image_loaded(ImageCache &cache, const std::string &mermaid_source, const std::string &font_path, std::optional<ThemeKind> theme);
