#include "app/app_state.h"

#include <algorithm>
#include <filesystem>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

// Layout of slide `slide_index` at the current window size — used both by
// the "normal" relayout (resize/hot-reload) and to capture both sides of a
// transition (see frame()).
TextLayoutResult compute_layout_for_slide(AppState &state, int slide_index) {
    int window_width  = GetScreenWidth();
    int window_height = GetScreenHeight();

    float initial_font_size = state.config.default_font_size * (static_cast<float>(window_height) / static_cast<float>(state.config.default_window_height));

    float available_width  = static_cast<float>(window_width) - 2.0f * state.config.margin_x;
    float available_height = static_cast<float>(window_height) - 2.0f * state.config.margin_y;

    return compute_fitted_layout(state.deck.slides[slide_index].content, state.renderer.fonts, state.config, initial_font_size, available_width, available_height, state.image_cache, state.renderer.font_paths.regular, state.args.theme);
}

// Renders a slide that already has its layout computed into `target` (one
// of the two transition buffers, or the zoom buffer) — used to capture
// both sides ("from"/"to") of a transition, and the zoomed view, without
// needing to redraw the content on every frame of the animation/zoom.
void render_slide_into_texture(AppState &state, RenderTexture2D target, const TextLayoutResult &slide_layout, const SlideParams &slide_params, int window_width, int window_height) {
    BeginTextureMode(target);
    ClearBackground(slide_params.bg_color.value_or(state.config.background_color));
    draw_centered_text(state.renderer, slide_layout, window_width, window_height, state.config, slide_params);
    EndTextureMode();
}

#ifdef __EMSCRIPTEN__
// Stops the browser's callback loop and releases everything once
// should_quit is set (ESC, or a finished --screenshot capture) — there's
// no equivalent of the native build falling out of its while loop and
// reaching the cleanup after it, so frame() has to do it here instead.
// Deliberately does NOT also check WindowShouldClose(): raylib's own
// PLATFORM_WEB implementation of it (rcore_web.c) calls emscripten_sleep()
// every time it's called, which aborts at runtime unless the whole program
// is built with -sASYNCIFY — its own comment says as much ("WindowShouldClose()
// is not called on a web-ready raylib application if using
// emscripten_set_main_loop()"). should_quit (ESC/screenshot) is the only
// exit path this build needs; there's no window-close button to catch.
void quit_if_requested(AppState &state) {
    if (!state.should_quit) {
        return;
    }
    emscripten_cancel_main_loop();
    shutdown(state);
    g_app_state = nullptr;
    delete &state;
}
#endif

void draw_waiting_for_file_screen(const AppState &state) {
    BeginDrawing();
    ClearBackground(state.config.background_color);

    const char *message  = "Select a .md file to begin";
    int font_size         = 24;
    int text_width         = MeasureText(message, font_size);
    DrawText(message, (GetScreenWidth() - text_width) / 2, (GetScreenHeight() - font_size) / 2, font_size, state.config.text_color);

    EndDrawing();
}

} // namespace

bool try_open_document(AppState &state, const std::string &path) {
    std::optional<Document> loaded = load_document(path);
    if (!loaded) {
        return false;
    }

    state.document    = loaded;
    state.base_dir    = std::filesystem::path(path).parent_path().string();
    state.deck        = build_slide_deck(state.document->text, state.base_dir);
    state.watch_state = make_file_watch_state(path); // (re)start watching from this file's current mtime
    ensure_styles_loaded(state.renderer, state.deck.usage);
    ensure_extra_fonts_loaded(state.renderer, state.deck.codepoints);

    state.has_document   = true;
    state.current_slide  = std::min(state.current_slide, static_cast<int>(state.deck.slides.size()) - 1);
    state.needs_relayout = true;
    reset_content_zoom(state.zoom_state); // content changed under it — start clean

    // The content changed — the grid's thumbnails no longer match it. If
    // the grid is open right now, rebuild immediately (otherwise stale
    // thumbnails would stay visible); otherwise just mark it dirty and
    // leave it to rebuild the next time the grid opens.
    state.overview_state.thumbnails_dirty = true;
    state.overview_state.focused_index    = std::min(state.overview_state.focused_index, static_cast<int>(state.deck.slides.size()) - 1);
    if (state.overview_state.active) {
        rebuild_overview_thumbnails(state.overview_state, state.deck, state.renderer, state.config, state.image_cache, state.args.theme);
    }

    return true;
}

void frame(void *arg) {
    AppState &state = *static_cast<AppState *>(arg);

    // --- Update ---
    if (state.has_document && poll_file_watch(state.watch_state, GetTime())) {
        try_open_document(state, state.args.source_path);
    }

    if (IsWindowResized()) {
        state.needs_relayout = true;
        // The transition buffers and the zoom buffer all need to match the
        // window size (see update_and_draw_transition / draw_zoomed_content)
        // — recreate them at the new size.
        UnloadRenderTexture(state.transition_from_buffer);
        UnloadRenderTexture(state.transition_to_buffer);
        UnloadRenderTexture(state.zoom_buffer);
        state.transition_from_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        state.transition_to_buffer   = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        state.zoom_buffer            = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        reset_content_zoom(state.zoom_state); // simpler than re-deriving a sensible pivot/pan at the new size
    }

    // A resize or hot-reload in the middle of a transition invalidates what
    // was captured (size or content changed) — cancel instead of trying to
    // keep animating something stale.
    if (state.needs_relayout && state.transition_state.active) {
        cancel_transition(state.transition_state);
    }

    if (!state.has_document) {
        // Emscripten only (see AppState::has_document) — nothing to
        // update/draw yet, the rest of this function assumes
        // state.deck.slides is non-empty.
        draw_waiting_for_file_screen(state);
#ifdef __EMSCRIPTEN__
        quit_if_requested(state);
#endif
        return;
    }

    // "Overview" mode: holding Ctrl (either side) opens the thumbnail grid;
    // while active, mouse/arrows/wheel control the focus (see
    // update_slide_overview) instead of the normal navigation below.
    // Releasing Ctrl (both sides, see confirmed_by_ctrl_release) or clicking
    // a thumbnail confirms the focused slide and closes the grid — always
    // with an instant cut, without the destination slide's transition
    // effect (there's no natural "direction" between non-adjacent slides).
    // The !overview_state.active condition avoids reopening/resetting focus
    // and scroll if the grid was already active for another reason (e.g.
    // --force-overview, see main.cpp) and the user presses Ctrl for the
    // first time while inside it just to use the release-to-confirm
    // gesture — without this, the focus the user had already chosen with
    // the mouse/arrows would be discarded.
    if ((IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) && !state.overview_state.active) {
        state.overview_state.active        = true;
        state.overview_state.focused_index = state.current_slide;
        state.overview_state.scroll_offset = 0.0f;
        reset_content_zoom(state.zoom_state); // the grid shows every slide at once — zoom doesn't apply there
        if (state.overview_state.thumbnails_dirty) {
            rebuild_overview_thumbnails(state.overview_state, state.deck, state.renderer, state.config, state.image_cache, state.args.theme);
        }
    }

    // Content zoom (see render/content_zoom.h) only makes sense on the
    // "real" slide — skipped while the overview grid is open (its own
    // wheel/click handling takes over, see update_slide_overview) or while
    // a transition is animating between two slides.
    if (!state.overview_state.active && !state.transition_state.active) {
        update_content_zoom(state.zoom_state, GetScreenWidth(), GetScreenHeight());
    }

    // ESC: first closes the zoom if the content is currently zoomed (see
    // consume_escape_for_content_zoom), otherwise it's the usual "close the
    // window" request — SetExitKey(KEY_NULL) in main.cpp disabled raylib's
    // own handling of this key, so this is the only place ESC is acted on.
    if (IsKeyPressed(KEY_ESCAPE) && !consume_escape_for_content_zoom(state.zoom_state)) {
        state.should_quit = true;
    }

    if (state.overview_state.active) {
        update_slide_overview(state.overview_state, state.deck, GetScreenWidth(), GetScreenHeight());

        bool confirmed_by_click = overview_consume_click(state.overview_state, GetScreenWidth(), GetScreenHeight(), state.deck);
        // Only confirms when NEITHER side of Ctrl is still pressed — holding
        // both down and releasing only one shouldn't close the grid too early.
        bool confirmed_by_ctrl_release = (IsKeyReleased(KEY_LEFT_CONTROL) || IsKeyReleased(KEY_RIGHT_CONTROL)) && !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL);

        if (confirmed_by_click || confirmed_by_ctrl_release) {
            state.current_slide         = std::clamp(state.overview_state.focused_index, 0, static_cast<int>(state.deck.slides.size()) - 1);
            state.overview_state.active = false;
            state.needs_relayout        = true;
        }
    }

    // Ignores navigation while a transition is playing — simpler than
    // interrupting/queuing, and the effect is short-lived (see
    // AppConfig::transition_duration_seconds). Also ignores it while the
    // thumbnail grid is active — in that case arrows control the grid's
    // focus, they don't advance/go back a slide underneath it.
    if (!state.transition_state.active && !state.overview_state.active) {
        int next_slide = -1;
        int direction  = 0;
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) && state.current_slide + 1 < static_cast<int>(state.deck.slides.size())) {
            next_slide = state.current_slide + 1;
            direction  = 1;
        } else if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) && state.current_slide > 0) {
            next_slide = state.current_slide - 1;
            direction  = -1;
        }

        if (next_slide != -1) {
            int window_width          = GetScreenWidth();
            int window_height         = GetScreenHeight();
            TransitionKind transition = state.deck.slides[next_slide].params.transition.value_or(state.args.default_transition);

            // Only captures the "from" side if it's actually going to animate —
            // saves two off-screen renders for nothing when the next slide
            // doesn't request a transition (transition=none, the usual
            // behavior: instant cut).
            if (transition != TransitionKind::None) {
                render_slide_into_texture(state, state.transition_from_buffer, state.layout, state.deck.slides[state.current_slide].params, window_width, window_height);
            }

            state.current_slide  = next_slide;
            state.layout         = compute_layout_for_slide(state, state.current_slide);
            state.needs_relayout = false;
            reset_content_zoom(state.zoom_state); // zoom is a per-slide inspection tool, doesn't carry to the next one

            if (transition != TransitionKind::None) {
                const SlideParams &new_params = state.deck.slides[state.current_slide].params;
                render_slide_into_texture(state, state.transition_to_buffer, state.layout, new_params, window_width, window_height);
                Color to_background = new_params.bg_color.value_or(state.config.background_color);
                begin_transition(state.transition_state, transition, direction, to_background, GetTime());
            }
        }
    }

    if (state.needs_relayout) {
        state.layout         = compute_layout_for_slide(state, state.current_slide);
        state.needs_relayout = false;
    }

    // Captures the current slide into zoom_buffer whenever it'll be needed
    // this frame — kept outside BeginDrawing/EndDrawing below, like the
    // transition buffers above, rather than nesting BeginTextureMode inside
    // the screen's own Begin/EndDrawing pair.
    if (!state.overview_state.active && !state.transition_state.active && is_content_zoomed(state.zoom_state)) {
        render_slide_into_texture(state, state.zoom_buffer, state.layout, state.deck.slides[state.current_slide].params, GetScreenWidth(), GetScreenHeight());
    }

    // --- Render ---
    BeginDrawing();
    if (state.overview_state.active) {
        // The grid's background uses the color of the slide currently being
        // shown (not a fixed generic color), so opening the grid doesn't
        // break visual continuity.
        Color current_bg = state.deck.slides[state.current_slide].params.bg_color.value_or(state.config.background_color);
        draw_slide_overview(state.overview_state, GetScreenWidth(), GetScreenHeight(), current_bg, state.config.code_background_color);
    } else if (state.transition_state.active) {
        update_and_draw_transition(state.transition_state, state.config, GetTime(), state.transition_from_buffer, state.transition_to_buffer);
    } else {
        const SlideParams &slide_params = state.deck.slides[state.current_slide].params;
        ClearBackground(slide_params.bg_color.value_or(state.config.background_color));
        if (is_content_zoomed(state.zoom_state)) {
            draw_zoomed_content(state.zoom_state, state.zoom_buffer, GetScreenWidth(), GetScreenHeight());
        } else {
            draw_centered_text(state.renderer, state.layout, GetScreenWidth(), GetScreenHeight(), state.config, slide_params);
        }
    }
    EndDrawing();

    // --screenshot: waits for things to settle, then exits, to inspect the
    // result without manual interaction (see cli/cli_args.h). Uses
    // LoadImageFromScreen + ExportImage instead of TakeScreenshot:
    // TakeScreenshot always concatenates the path with raylib's base
    // directory internally, breaking absolute paths. On Emscripten this
    // writes into the in-memory virtual filesystem, which isn't visible or
    // persisted anywhere the user can reach — the flag is native-CLI-only
    // in practice.
    if (!state.args.screenshot_path.empty() && GetTime() - state.loop_entry_time >= state.args.screenshot_delay) {
        Image screen = LoadImageFromScreen();
        ExportImage(screen, state.args.screenshot_path.c_str());
        UnloadImage(screen);
        state.should_quit = true;
    }

#ifdef __EMSCRIPTEN__
    quit_if_requested(state);
#endif
}

void shutdown(AppState &state) {
    cancel_transition(state.transition_state);
    UnloadRenderTexture(state.transition_from_buffer);
    UnloadRenderTexture(state.transition_to_buffer);
    UnloadRenderTexture(state.zoom_buffer);
    unload_slide_overview(state.overview_state);
    unload_image_cache(state.image_cache);
    unload_text_renderer(state.renderer);
    CloseWindow();
}

#ifdef __EMSCRIPTEN__
AppState *g_app_state = nullptr;
#endif
