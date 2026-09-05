#include "platform/default_font.h"

#include <raylib.h>

#include <cstdio>

namespace {

// Applies the same rule to any variant: if `found` actually exists, use
// it; otherwise fall back to `fallback` (the already-resolved regular
// font) and log it.
std::string resolve_or_fallback(const std::string& found, const std::string& fallback, const char* variant_name) {
  if (!found.empty() && FileExists(found.c_str())) {
    return found;
  }
  std::fprintf(stderr, "Warning: system %s not found; using the regular font.\n", variant_name);
  return fallback;
}

}  // namespace

FontPaths resolve_font_paths(const std::string& bundled_font_path) {
  FontPaths paths;

  std::string regular = get_system_default_font_path();
  if (regular.empty() || !FileExists(regular.c_str())) {
    std::fprintf(stderr, "Warning: system default font not found; using the bundled font.\n");
    regular = bundled_font_path;
  }
  paths.regular = regular;

  paths.bold = resolve_or_fallback(get_system_bold_font_path(), regular, "bold font");
  paths.italic = resolve_or_fallback(get_system_italic_font_path(), regular, "italic font");
  paths.bold_italic = resolve_or_fallback(get_system_bold_italic_font_path(), regular, "bold-italic font");

  // Different from the others: without a real monospace font, it makes
  // no sense to fall back to the regular font (it isn't monospace) — we
  // leave it empty for the caller to use raylib's built-in font.
  std::string mono = get_system_monospace_font_path();
  if (mono.empty() || !FileExists(mono.c_str())) {
    std::fprintf(stderr, "Warning: system monospace font not found; using raylib's built-in font.\n");
    mono.clear();
  }
  paths.mono = mono;

  return paths;
}
