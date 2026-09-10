#pragma once

#include <raylib.h>

#include <optional>
#include <string>

#include "assets/image_cache.h"
#include "cli/cli_args.h"
#include "config/config.h"
#include "document/document.h"
#include "render/content_zoom.h"
#include "render/slide_overview.h"
#include "render/slide_transition.h"
#include "render/text_renderer.h"
#include "slides/slide_deck.h"
#include "text/text_layout.h"
#include "watch/file_watch.h"

// Everything the frame-by-frame update/render logic (see frame()) needs
// across calls. The native build keeps this on main()'s stack, inside its
// blocking while loop; the Emscripten build allocates it on the heap (see
// main.cpp) because emscripten_set_main_loop_arg's callback runs after
// main() has already returned control to the browser — it has no access
// to main()'s stack frame at that point.
struct AppState {
    CliArgs args;
    AppConfig config;

    TextRenderer renderer;
    ImageCache image_cache;
    FileWatchState watch_state; // only meaningful once has_document is true

    // Directory of the currently open .md file, used to resolve relative
    // image paths (see assets/image_cache.h::resolve_image_path) — empty
    // (resolves relative to the current working directory) when the path
    // given to try_open_document has no directory component.
    std::string base_dir;

    // False until try_open_document succeeds at least once. Only reachable
    // on the Emscripten build (the browser starts with no file selected —
    // see main.cpp/platform/wasm_bridge.cpp); the native CLI always has a
    // document by the time the loop starts, since main.cpp fails fast if
    // the file given on the command line can't be read.
    bool has_document = false;
    std::optional<Document> document;
    SlideDeck deck;

    int current_slide = 0;
    bool needs_relayout = false;
    TextLayoutResult layout;
    double loop_entry_time = 0.0;
    SlideTransitionState transition_state;

    // Pair of RenderTexture2D reused across every transition (see
    // render/slide_transition.h) — same reasoning as the original main.cpp
    // loop: recreating one on every slide change caused a brief stutter.
    RenderTexture2D transition_from_buffer{};
    RenderTexture2D transition_to_buffer{};

    ContentZoomState zoom_state;
    RenderTexture2D zoom_buffer{};

    // True once ESC (with nothing left to reset via content zoom) or the
    // window's close button has requested an exit; on Emscripten also set
    // after a --screenshot capture (see frame()), where it additionally
    // triggers emscripten_cancel_main_loop()+shutdown().
    bool should_quit = false;

    SlideOverviewState overview_state;
};

// Loads (or reloads) the document at `path`: reads it, rebuilds the slide
// deck (base_dir is derived from `path`), loads any newly-needed font
// styles/codepoints, and resets the bits of per-document UI state that
// don't make sense to carry over (content zoom, dirty overview
// thumbnails). Leaves `state` otherwise untouched and returns false if the
// file can't be read. Used for the initial load and for every later
// (re)load alike: native hot-reload (frame(), driven by
// watch/file_watch.h) and the Emscripten browser file picker
// (platform/wasm_bridge.cpp) both call this same function.
bool try_open_document(AppState &state, const std::string &path);

// One frame's worth of update + render — the former body of main.cpp's
// while loop, extracted so it can be driven either by a blocking while
// loop (native) or by emscripten_set_main_loop_arg (Web), see main.cpp.
// `arg` is an AppState*.
void frame(void *arg);

// Releases every GPU resource held by `state` (render textures, overview
// thumbnails, image cache, fonts/shader) and closes the window. Called
// once the loop has ended: after the native while loop returns, or from
// inside frame() itself on Emscripten once should_quit is set.
void shutdown(AppState &state);

#ifdef __EMSCRIPTEN__
// The single AppState instance for this run, set by main.cpp right after
// allocating it — platform/wasm_bridge.cpp uses this to reach the running
// app from astral_open_document, called from JS once the browser file
// picker has written a document into the virtual filesystem. Not used on
// native builds (no JS bridge calls into a running native process).
extern AppState *g_app_state;
#endif
