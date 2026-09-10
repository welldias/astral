#pragma once

#include <optional>
#include <string>

#include "config/theme.h"
#include "slides/slide_params.h"

struct CliArgs {
    std::string source_path;

    // Slide number (1 = first) to open directly on, instead of the start.
    // Values outside the range of existing slides are clamped to the
    // closest valid one (see main.cpp) — not an error.
    int initial_slide;

    // Empty: runs normally (interactive window, until the user closes it).
    // Filled in: waits for the window to settle, saves a screenshot to that
    // path, and exits — useful for inspecting the result of a change
    // without having to interact with the window.
    std::string screenshot_path;
    double screenshot_delay;

    // Transition effect used when navigating between slides that don't
    // specify the "transition" parameter in their own marker (see
    // slides/slide_splitter.h::SlideParams::transition) — defaults to None
    // (instant cut, the long-standing behavior). A slide with an explicit
    // "transition=..." in its marker always takes priority over this
    // default.
    TransitionKind default_transition;

    // Path to a dedicated font for drawing emoji / Asian-language
    // characters (CJK, Hiragana/Katakana, Hangul, ...) — see
    // text/glyph_class.h::GlyphFontKind. Empty: no font was passed; main.cpp
    // then looks for the deck's own "emoji"/"unifont".ttf|.otf next to its
    // .md file (see platform/default_font.h::find_font_in_directory) —
    // "unifont" rather than "asian" since GNU Unifont is the usual choice
    // for this coverage. Neither given nor found: characters in these
    // categories fall back to the regular font, which doesn't have that
    // glyph (shows up as '?', see raylib::GetGlyphIndex), and each one
    // produces a warning on stderr the first time it appears (see
    // render/text_renderer.h::ensure_extra_fonts_loaded). Loaded on demand,
    // only if the document actually uses a character from the corresponding
    // category (same scheme as bold/italic/mono in
    // render/text_renderer.h::ensure_styles_loaded).
    std::string emoji_font_path;
    std::string asian_font_path;

    // Paths to user-chosen fonts for the regular/italic/bold/monospace
    // variants of the main text (see platform/default_font.h::FontPaths) —
    // --regular-font/--italic-font/--bold-font/--mono-font. Empty: not
    // given; for regular/italic/bold, main.cpp then looks for the deck's own
    // "default"/"italic"/"bold".ttf|.otf next to its .md file (see
    // platform/default_font.h::find_font_in_directory) before finally
    // falling back to the operating system's own default font for that
    // variant (the long-standing behavior, see resolve_font_paths); mono has
    // no such deck-local lookup, only the OS fallback. Given but pointing to
    // a file that doesn't exist is a hard error (see
    // main.cpp) — unlike a missing OS font, there's no sensible fallback for
    // a font the user explicitly asked for by path. Doesn't cover
    // bold-italic: that combination has no dedicated flag; it's resolved
    // from the operating system as before UNLESS --regular-font is given, in
    // which case it's derived from whichever of --bold-font/--italic-font
    // was also given (or the regular font itself), with the missing
    // weight/slant synthesized at draw time instead of pulled from an
    // unrelated system font (see main.cpp and
    // render/text_renderer.h::TextRenderer::synth_bold/synth_italic/
    // synth_bold_italic_shear).
    std::string regular_font_path;
    std::string italic_font_path;
    std::string bold_font_path;
    std::string mono_font_path;

    // true: the presentation opens directly in overview mode (thumbnail
    // grid, see render/slide_overview.h) instead of going straight to the
    // initial slide — useful for choosing where to start presenting by
    // seeing all slides at once. Exits the mode normally (releasing Ctrl,
    // or clicking a thumbnail — see main.cpp), even without having had to
    // hold Ctrl to open it.
    bool force_overview = false;

    // Built-in color theme selected via "--theme <name>" (see
    // config/theme.h::parse_theme_kind for the accepted names) — overrides
    // AppConfig's default colors (see main.cpp). nullopt: not given, keeps
    // Astral's long-standing default colors (make_default_config).
    std::optional<ThemeKind> theme;

    // false (default): raylib's own TraceLog output and Astral's own
    // diagnostic warnings (see platform/log.h) are both silenced during
    // startup, and a loading progress bar is drawn in their place (see
    // main.cpp) — the long-standing wall of log lines was noise for anyone
    // not actively debugging a load issue. true ("--verbose-log"): restores
    // that log output exactly as before, and skips the progress bar (the
    // two aren't drawn together — the log lines are the feedback instead).
    bool verbose_log = false;
};

// Usage: astral <file> [--slide <number>] [--screenshot <path.png>]
//      [--screenshot-delay <seconds>] [--transition <fade|slide|zoom|none>]
//      [--emoji-font <path.ttf>] [--asian-font <path.ttf>]
//      [--regular-font <path.ttf>] [--italic-font <path.ttf>]
//      [--bold-font <path.ttf>] [--mono-font <path.ttf>] [--force-overview]
//      [--theme <name>] [--verbose-log]
// Returns std::nullopt if the arguments are invalid (missing file, a
// flag without its expected value, or an unrecognized --transition or
// --theme value) — the caller should print the usage message. Doesn't
// check that --regular-font/--italic-font/--bold-font/--mono-font actually
// exist — that's a hard error, not a usage error, and is checked by
// main.cpp once the source file's own existence has already been
// confirmed.
std::optional<CliArgs> parse_cli_args(int argc, char **argv);
