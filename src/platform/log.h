#pragma once

// Astral's own diagnostic warnings (font fallbacks, missing glyphs — see
// platform/default_font_resolve.cpp and render/text_renderer.cpp), as
// opposed to raylib's own TraceLog output (gated separately via
// SetTraceLogLevel, see main.cpp). Both are silenced by default and only
// shown with --verbose-log (see cli/cli_args.h): a plain run shows the
// loading progress bar instead of a wall of text scrolling by.

// Sets whether log_warning actually prints — called once at startup from
// --verbose-log (see main.cpp), before any loading that might warn.
void set_verbose_log_enabled(bool enabled);

// Same shape as fprintf(stderr, ...), but only prints while verbose
// logging is enabled (see set_verbose_log_enabled). Always prefix `format`
// with "Warning: " at the call site, matching the messages this replaces.
void log_warning(const char *format, ...);
