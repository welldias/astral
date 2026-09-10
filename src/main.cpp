#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "app/app_state.h"
#include "cli/cli_args.h"
#include "config/config.h"
#include "config/theme.h"
#include "document/document.h"
#include "platform/default_font.h"
#include "platform/log.h"
#include "render/slide_overview.h"
#include "render/text_renderer.h"

namespace {

constexpr const char *kUsage =
    "Usage: %s <file> [--slide <number>] [--screenshot <path.png>] [--screenshot-delay "
    "<seconds>] [--transition <fade|slide|zoom|none>] [--emoji-font <path.ttf>] [--asian-font "
    "<path.ttf>] [--regular-font <path.ttf>] [--italic-font <path.ttf>] [--bold-font <path.ttf>] "
    "[--mono-font <path.ttf>] [--force-overview] [--theme <name>] [--verbose-log]\n";

// Draws one frame of a loading progress bar, centered on the window — shown
// in place of the log output that startup would otherwise print (see
// platform/log.h and the SetTraceLogLevel call below), so the user gets
// some feedback instead of a silent, seemingly-frozen window. Only called
// with --verbose-log absent (see main()); with it, the log lines
// themselves are the feedback, and calling this too would just interleave
// a progress bar into the middle of the terminal output.
void draw_loading_progress(const AppConfig &config, float fraction) {
    BeginDrawing();
    ClearBackground(config.background_color);

    int window_width  = GetScreenWidth();
    int window_height = GetScreenHeight();

    float bar_width  = static_cast<float>(window_width) * 0.4f;
    float bar_height = 6.0f;
    float bar_x      = (static_cast<float>(window_width) - bar_width) / 2.0f;
    float bar_y      = static_cast<float>(window_height) / 2.0f;

    Color track_color = Fade(config.text_color, 0.25f);
    Rectangle track    = { bar_x, bar_y, bar_width, bar_height };
    Rectangle fill     = { bar_x, bar_y, bar_width * std::clamp(fraction, 0.0f, 1.0f), bar_height };
    DrawRectangleRounded(track, 1.0f, 8, track_color);
    if (fill.width > 0.0f) {
        DrawRectangleRounded(fill, 1.0f, 8, config.text_color);
    }

    const char *label = "Loading...";
    int label_size     = 20;
    int label_width    = MeasureText(label, label_size);
    DrawText(label, static_cast<int>(bar_x + (bar_width - static_cast<float>(label_width)) / 2.0f), static_cast<int>(bar_y - static_cast<float>(label_size) - 14.0f), label_size, config.text_color);

    EndDrawing();
}

} // namespace

int main(int argc, char **argv) {
    std::optional<CliArgs> parsed_args = parse_cli_args(argc, argv);

    CliArgs args;
    bool has_initial_document = false;
    if (parsed_args) {
        args                  = *parsed_args;
        has_initial_document  = true;
    }
#ifdef __EMSCRIPTEN__
    else if (argc <= 1) {
        // No arguments — the normal way the browser instantiates the
        // module (see web/shell.html, which starts it with no
        // Module.arguments) — start with no document loaded; wait for
        // astral_open_document (platform/wasm_bridge.cpp), triggered once
        // the user picks a file, instead of failing like the native CLI
        // does when no file is given.
        args.screenshot_delay   = 0.5;
        args.initial_slide      = 1;
        args.default_transition = TransitionKind::None;
    }
#endif
    else {
        std::fprintf(stderr, kUsage, argv[0]);
        return 1;
    }

    // Applies to Astral's own diagnostic warnings (platform/log.h) as well
    // as raylib's TraceLog (silenced below, right before InitWindow) — both
    // are noise unless the user actually asked to see them via
    // --verbose-log. Set as early as possible so nothing loaded before this
    // point could have logged anyway (font/window loading hasn't started
    // yet); the hard errors below (bad file, missing user-given font) are
    // fprintf'd directly rather than through log_warning, so they always
    // show regardless of this flag.
    set_verbose_log_enabled(args.verbose_log);
    bool show_loading_progress = !args.verbose_log;

    // Fails fast, before opening any window — matches the long-standing
    // native behavior. try_open_document (below) re-reads the file; that
    // small duplication keeps this early check simple, and the file is
    // small and read only once at startup either way. On Emscripten, a
    // document is only already given here in the unusual case of
    // Module.arguments being set up front (normally none are, see above)
    // — a failure there just falls through to the "waiting for a file"
    // screen instead of exiting the whole runtime.
#ifndef __EMSCRIPTEN__
    if (has_initial_document && !load_document(args.source_path)) {
        std::fprintf(stderr, "Error: could not read file '%s'\n", args.source_path.c_str());
        return 1;
    }
#endif

    // --regular-font/--italic-font/--bold-font/--mono-font (see
    // cli/cli_args.h): given but missing is a hard error, unlike a missing OS
    // font (resolve_font_paths just warns and falls back) — there's no
    // reasonable fallback for a font the user explicitly asked for by path
    // that doesn't exist.
    for (const auto &[path, flag_name] : {
             std::pair<const std::string &, const char *>{ args.regular_font_path, "--regular-font" },
                  std::pair<const std::string &, const char *>{ args.italic_font_path,  "--italic-font"  },
                  std::pair<const std::string &, const char *>{ args.bold_font_path,    "--bold-font"    },
                  std::pair<const std::string &, const char *>{ args.mono_font_path,    "--mono-font"    }
    }) {
        if (!path.empty() && !FileExists(path.c_str())) {
            std::fprintf(stderr, "Error: font given via %s ('%s') not found\n", flag_name, path.c_str());
            return 1;
        }
    }

    AppConfig config = make_default_config();
    if (args.theme) {
        apply_theme(config, *args.theme);
    }

    // raylib's own TraceLog (window/GPU/font/shader info, printed to stdout)
    // — silenced by default in favor of the loading progress bar drawn
    // below; --verbose-log leaves raylib's default level untouched, i.e.
    // this run behaves exactly like before that flag existed.
    if (!args.verbose_log) {
        SetTraceLogLevel(LOG_NONE);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(config.default_window_width, config.default_window_height, config.window_title.c_str());
    SetWindowMinSize(config.min_window_width, config.min_window_height);
    SetTargetFPS(60);

    if (show_loading_progress) {
        draw_loading_progress(config, 0.05f);
    }

    // Disables raylib's built-in "ESC closes the window" handling — ESC still
    // closes the window (see AppState::should_quit), but only once content
    // zoom (render/content_zoom.h) isn't active; the first ESC while zoomed
    // resets it instead. The window's own close button still works
    // regardless of this: WindowShouldClose() also tracks that
    // independently of the exit key.
    SetExitKey(KEY_NULL);

    FontPaths font_paths = resolve_font_paths(config.bundled_font_path);

    // A deck can ship its own default/bold/italic font next to its .md file
    // (see platform/default_font.h::find_font_in_directory) — picked up
    // automatically, with the same effect as the user having passed
    // --regular-font/--bold-font/--italic-font pointing at it. An explicit
    // flag always wins over the deck's own font, same file or not. Empty
    // deck_dir (no document path, e.g. Emscripten before a file is opened)
    // just means no lookup happens, same as a deck with no such files.
    std::string deck_dir;
    if (!args.source_path.empty()) {
        deck_dir = std::filesystem::path(args.source_path).parent_path().string();
        if (deck_dir.empty()) {
            deck_dir = "."; // bare filename (e.g. "demo.md"): its directory is cwd
        }
    }
    std::string effective_regular_font = !args.regular_font_path.empty() ? args.regular_font_path : find_font_in_directory(deck_dir, "default");
    std::string effective_bold_font    = !args.bold_font_path.empty() ? args.bold_font_path : find_font_in_directory(deck_dir, "bold");
    std::string effective_italic_font  = !args.italic_font_path.empty() ? args.italic_font_path : find_font_in_directory(deck_dir, "italic");

    // Same deck-local lookup for --emoji-font/--asian-font — "emoji.ttf" and
    // "unifont.ttf" specifically (not "asian.ttf": GNU Unifont, the usual
    // choice for CJK/Hiragana/Katakana/Hangul coverage, is the name already
    // used for this in demo/, so it doubles as the deck-local convention).
    std::string effective_emoji_font = !args.emoji_font_path.empty() ? args.emoji_font_path : find_font_in_directory(deck_dir, "emoji");
    std::string effective_asian_font = !args.asian_font_path.empty() ? args.asian_font_path : find_font_in_directory(deck_dir, "unifont");

    // User-chosen (or deck-local, see above) fonts take priority over
    // whatever resolve_font_paths found on the operating system — applied
    // per variant, independently: only a bold font, say, leaves
    // regular/italic/mono on the OS's own default.
    //
    // The regular one is different: once a specific custom face is in play
    // for it, pairing it with whatever bold/italic happens to be installed
    // on the OS would look inconsistent (different family entirely). So
    // instead, any variant not also given its own real font is aliased to
    // the custom regular font and synthesized at draw time instead
    // (faux-bold via the SDF shader, synthetic oblique via a shear
    // transform — see render/text_renderer.cpp's
    // kSyntheticBoldAmount/kSyntheticItalicShear and
    // TextRenderer::synth_bold/synth_italic/synth_bold_italic_shear).
    // Without a custom regular font, none of this triggers and behavior is
    // exactly as before: resolve_font_paths' own OS-fallback (aliasing to
    // the system regular font, undecorated) stands.
    bool custom_regular_font = !effective_regular_font.empty();
    bool has_real_bold_font  = !effective_bold_font.empty();
    bool has_real_italic_font = !effective_italic_font.empty();

    if (custom_regular_font) {
        font_paths.regular = effective_regular_font;
        if (!has_real_italic_font) {
            font_paths.italic = font_paths.regular;
        }
        if (!has_real_bold_font) {
            font_paths.bold = font_paths.regular;
        }
    }
    if (has_real_italic_font) {
        font_paths.italic = effective_italic_font;
    }
    if (has_real_bold_font) {
        font_paths.bold = effective_bold_font;
    }
    if (!args.mono_font_path.empty()) {
        font_paths.mono = args.mono_font_path;
    }

    // bold_italic has no dedicated CLI flag (see cli/cli_args.h): outside
    // the custom-regular-font pipeline it keeps following the OS/regular
    // fallback exactly as resolve_font_paths already set it up. Inside that
    // pipeline, basing it on an unrelated system bold-italic face would be
    // just as inconsistent as for bold/italic above, so it's anchored to
    // whichever real face the user did supply instead — a real bold font
    // takes priority as the base (only its slant then needs synthesizing;
    // widening an already-bold weight further is unnecessary), then a real
    // italic font (only its weight needs synthesizing), then the regular
    // font itself (needs both).
    bool synth_bold   = custom_regular_font && !has_real_bold_font;
    bool synth_italic = custom_regular_font && !has_real_italic_font;
    // fonts.bold_italic never uses the real italic file as its base unless
    // there's no real bold font either — so it needs the shear fallback
    // whenever a real bold font isn't already its base.
    bool synth_bold_italic_shear = custom_regular_font && !(has_real_italic_font && !has_real_bold_font);

    if (custom_regular_font) {
        if (has_real_bold_font) {
            font_paths.bold_italic = font_paths.bold;
        } else if (has_real_italic_font) {
            font_paths.bold_italic = font_paths.italic;
        } else {
            font_paths.bold_italic = font_paths.regular;
        }
    }

    if (show_loading_progress) {
        draw_loading_progress(config, 0.15f);
    }

    // Heap-allocated (rather than a local variable) so the same AppState
    // outlives main() itself — required on Emscripten, where
    // emscripten_set_main_loop_arg's callback (frame(), see app/app_state.h)
    // keeps running after main() returns control to the browser. The native
    // build could just as well use a stack variable, but sharing one
    // AppState/frame() between both platforms keeps the two builds from
    // drifting apart.
    AppState *state = new AppState();
#ifdef __EMSCRIPTEN__
    g_app_state = state; // platform/wasm_bridge.cpp reaches the running app through this
#endif
    state->args             = args;
    state->config           = config;
    state->renderer         = load_text_renderer(font_paths, config.font_atlas_base_size, effective_emoji_font, effective_asian_font, synth_bold, synth_italic, synth_bold_italic_shear);
    state->loop_entry_time  = GetTime();

    if (show_loading_progress) {
        draw_loading_progress(config, 0.7f);
    }

    // Pair of RenderTexture2D reused across every transition (see
    // render/slide_transition.h) — allocating a new one on every slide
    // change already caused a brief stutter/flicker right at the moment of
    // the change (GPU allocation mid-frame); by keeping the same pair alive
    // and only rewriting its contents, the allocation cost is only paid
    // here and on an actual resize, never during navigation.
    state->transition_from_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    state->transition_to_buffer   = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    // Interactive content zoom (see render/content_zoom.h) — reuses the same
    // "render the slide into a texture, then draw that texture" approach as
    // the transition buffers above, for the same reason.
    state->zoom_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    if (show_loading_progress) {
        draw_loading_progress(config, 0.8f);
    }

    if (has_initial_document) {
        if (!try_open_document(*state, args.source_path)) {
#ifndef __EMSCRIPTEN__
            // Unreachable in practice (already confirmed readable above),
            // kept as a defensive fallback for a file removed in between.
            std::fprintf(stderr, "Error: could not read file '%s'\n", args.source_path.c_str());
            shutdown(*state);
            delete state;
            return 1;
#endif
            // Emscripten: falls through to the "waiting for a file" screen.
        } else {
            // --slide is 1-based (natural for someone navigating a
            // presentation); outside the range of existing slides, it
            // clamps to the nearest valid one. try_open_document already
            // clamped current_slide to the deck's size (a no-op here, since
            // it starts at 0), so this is the only place initial_slide
            // actually applies.
            state->current_slide = std::clamp(args.initial_slide - 1, 0, static_cast<int>(state->deck.slides.size()) - 1);

            if (show_loading_progress) {
                draw_loading_progress(config, 0.95f);
            }

            // "Overview" mode (clickable grid of thumbnails, see
            // render/slide_overview.h) — activated while Ctrl is held down
            // (see app/app_state.cpp). --force-overview starts the
            // presentation already in this mode, with all slides visible at
            // once, instead of opening directly on the initial slide.
            if (args.force_overview) {
                state->overview_state.active        = true;
                state->overview_state.focused_index = state->current_slide;
                rebuild_overview_thumbnails(state->overview_state, state->deck, state->renderer, state->config, state->image_cache, args.theme);
            }
        }
    }

    if (show_loading_progress) {
        draw_loading_progress(config, 1.0f);
    }

#ifdef __EMSCRIPTEN__
    // 0 = drive the loop from requestAnimationFrame (the browser's own
    // refresh rate) instead of a fixed interval; true = simulate_infinite_
    // loop, so this call doesn't return until the loop is cancelled (see
    // frame(), which calls emscripten_cancel_main_loop()+shutdown() itself
    // once should_quit is set — main.cpp never gets to the cleanup below on
    // this build).
    emscripten_set_main_loop_arg(frame, state, 0, true);
#else
    while (!WindowShouldClose() && !state->should_quit) {
        frame(state);
    }

    shutdown(*state);
    delete state;
#endif

    return 0;
}
