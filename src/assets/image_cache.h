#pragma once

#include <raylib.h>

#include <string>
#include <unordered_map>

// Cache of textures for images referenced by Markdown image blocks
// (![alt](path), see BlockKind::Image in markdown/markdown_parser.h), plus
// (mermaid_loaded/mermaid_failed) textures rendered from ```mermaid
// diagram blocks (see BlockKind::Mermaid and render/mermaid_render.h).
// Loads each already-resolved path (or, for mermaid, each distinct
// diagram source) exactly once and keeps the texture on the GPU until
// unload_image_cache — there's no invalidation on document hot-reload: if
// the image file changes on disk after already being loaded, the old
// version stays in use until the process restarts (only the .md file has
// change watching, see watch/file_watch.h).
struct ImageCache {
    std::unordered_map<std::string, Texture2D> loaded; // resolved path -> texture
    std::unordered_map<std::string, bool> failed;      // resolved path already tried and unsuccessful

    // Same idea, for diagrams rendered by render/mermaid_render.h
    // (BlockKind::Mermaid) — key is the diagram's raw source text (there's
    // no file path to key on) instead of a resolved path. Kept as separate
    // maps rather than sharing loaded/failed above, since the two key
    // spaces (file paths vs. diagram source text) are conceptually
    // unrelated and could theoretically collide.
    std::unordered_map<std::string, Texture2D> mermaid_loaded;
    std::unordered_map<std::string, bool> mermaid_failed;
};

// Resolves `image_path` (as written in the Markdown) relative to
// `base_dir` (the .md file's directory) when it's a relative path;
// an absolute path is returned as is. Empty `base_dir` (a .md file with
// no directory component in its path, e.g. run from its own folder)
// resolves relative to the current working directory.
std::string resolve_image_path(const std::string &base_dir, const std::string &image_path);

// Ensures the image at `resolved_path` (which should already come from
// resolve_image_path) is loaded — idempotent: a path already tried,
// whether successfully or not, is not tried again. Returns a pointer to
// the loaded texture, valid until unload_image_cache (later insertions
// into the cache don't invalidate textures already returned — see
// std::unordered_map), or nullptr if the file doesn't exist or couldn't
// be read as an image; in that case the caller should use the block's
// alt text instead (see text/text_layout.h and render/text_renderer.cpp).
const Texture2D *ensure_image_loaded(ImageCache &cache, const std::string &resolved_path);

void unload_image_cache(ImageCache &cache);
