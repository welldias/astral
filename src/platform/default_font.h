#pragma once

#include <string>

// Each function below returns the path of a font file (.ttf/.otf) for
// the respective variant of the operating system's default font, or an
// empty string if that specific variant doesn't actually exist (the
// caller must not synthesize the effect — see resolve_font_paths). Each
// operating system has its own implementation (see
// default_font_linux.cpp / default_font_win.cpp / default_font_macos.cpp);
// only one of them is compiled, depending on the build's target OS.
std::string get_system_default_font_path();
std::string get_system_bold_font_path();
std::string get_system_italic_font_path();
std::string get_system_bold_italic_font_path();
std::string get_system_monospace_font_path();

struct FontPaths {
  std::string regular;
  std::string bold;
  std::string italic;
  std::string bold_italic;
  std::string mono;  // empty when the OS has no monospace font: use raylib's built-in font, not the regular one
};

// Resolves the 5 system font variants. Bold/italic/bold-italic follow
// the same rule: if not found, use the already-resolved regular font.
// The monospace one is different: if not found, `mono` is left empty
// (the caller must use raylib's built-in font, not the regular one —
// it isn't monospace). If the OS's own regular font isn't found, use
// `bundled_font_path`. Each fallback logs a warning to stderr.
// Implemented in default_font_resolve.cpp, compiled on every OS (doesn't
// depend on any platform-specific API).
FontPaths resolve_font_paths(const std::string& bundled_font_path);
