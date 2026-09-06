#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#include "assets/image_cache.h"
#include "cli/cli_args.h"
#include "config/config.h"
#include "document/document.h"
#include "platform/default_font.h"
#include "render/content_zoom.h"
#include "render/slide_overview.h"
#include "render/slide_transition.h"
#include "render/text_renderer.h"
#include "slides/slide_deck.h"
#include "text/text_layout.h"
#include "watch/file_watch.h"

int main(int argc, char** argv) {
  std::optional<CliArgs> args = parse_cli_args(argc, argv);
  if (!args) {
    std::fprintf(stderr,
                 "Usage: %s <file> [--slide <number>] [--screenshot <path.png>] [--screenshot-delay "
                 "<seconds>] [--transition <fade|slide|zoom|none>] [--emoji-font <path.ttf>] [--asian-font "
                 "<path.ttf>] [--regular-font <path.ttf>] [--italic-font <path.ttf>] [--bold-font <path.ttf>] "
                 "[--mono-font <path.ttf>] [--force-overview]\n",
                 argv[0]);
    return 1;
  }

  std::optional<Document> document = load_document(args->source_path);
  if (!document) {
    std::fprintf(stderr, "Error: could not read file '%s'\n", args->source_path.c_str());
    return 1;
  }

  // --regular-font/--italic-font/--bold-font/--mono-font (see
  // cli/cli_args.h): given but missing is a hard error, unlike a missing OS
  // font (resolve_font_paths below just warns and falls back) — there's no
  // reasonable fallback for a font the user explicitly asked for by path
  // that doesn't exist.
  for (const auto& [path, flag_name] :
       {std::pair<const std::string&, const char*>{args->regular_font_path, "--regular-font"},
        std::pair<const std::string&, const char*>{args->italic_font_path, "--italic-font"},
        std::pair<const std::string&, const char*>{args->bold_font_path, "--bold-font"},
        std::pair<const std::string&, const char*>{args->mono_font_path, "--mono-font"}}) {
    if (!path.empty() && !FileExists(path.c_str())) {
      std::fprintf(stderr, "Error: font given via %s ('%s') not found\n", flag_name, path.c_str());
      return 1;
    }
  }

  AppConfig config = make_default_config();

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(config.default_window_width, config.default_window_height, config.window_title.c_str());
  SetWindowMinSize(config.min_window_width, config.min_window_height);
  SetTargetFPS(60);

  // Disables raylib's built-in "ESC closes the window" handling — ESC still
  // closes the window (see should_quit below), but only once content zoom
  // (render/content_zoom.h) isn't active; the first ESC while zoomed resets
  // it instead. The window's own close button still works regardless of
  // this: WindowShouldClose() also tracks that independently of the exit key.
  SetExitKey(KEY_NULL);

  FontPaths font_paths = resolve_font_paths(config.bundled_font_path);

  // User-chosen fonts (already validated above) take priority over whatever
  // resolve_font_paths found on the operating system — applied per variant,
  // independently: passing only --bold-font, say, leaves regular/italic/mono
  // on the OS's own default. bold_italic is untouched: it has no dedicated
  // flag (see cli/cli_args.h) and keeps following the OS/regular-font
  // fallback exactly as resolve_font_paths already set it up. A non-empty
  // font_paths.mono is loaded as a real font regardless of source (see
  // ensure_styles_loaded) — --mono-font needs no special-casing here even
  // though an OS-resolved empty mono normally means "use raylib's built-in
  // font instead".
  if (!args->regular_font_path.empty()) {
    font_paths.regular = args->regular_font_path;
  }
  if (!args->italic_font_path.empty()) {
    font_paths.italic = args->italic_font_path;
  }
  if (!args->bold_font_path.empty()) {
    font_paths.bold = args->bold_font_path;
  }
  if (!args->mono_font_path.empty()) {
    font_paths.mono = args->mono_font_path;
  }

  TextRenderer renderer =
      load_text_renderer(font_paths, config.font_atlas_base_size, args->emoji_font_path, args->asian_font_path);
  FileWatchState watch_state = make_file_watch_state(args->source_path);
  ImageCache image_cache;

  // Directory of the .md file, used to resolve relative image paths
  // (see assets/image_cache.h::resolve_image_path) — empty (resolves
  // relative to the current working directory) when the given path has
  // no directory component, e.g. "slides.md".
  std::string base_dir = std::filesystem::path(args->source_path).parent_path().string();

  SlideDeck deck = build_slide_deck(document->text, base_dir);
  ensure_styles_loaded(renderer, deck.usage);
  ensure_extra_fonts_loaded(renderer, deck.codepoints);

  // --slide is 1-based (natural for someone navigating a presentation);
  // outside the range of existing slides, it clamps to the nearest valid one.
  int current_slide = std::clamp(args->initial_slide - 1, 0, static_cast<int>(deck.slides.size()) - 1);
  bool needs_relayout = true;
  TextLayoutResult layout;
  double loop_entry_time = GetTime();
  SlideTransitionState transition_state;

  // Pair of RenderTexture2D reused across every transition (see
  // render/slide_transition.h) — allocating a new RenderTexture2D on every
  // slide change already caused a brief stutter/flicker right at the
  // moment of the change (GPU allocation mid-frame); by keeping the same
  // pair alive and only rewriting its contents, the allocation cost is
  // only paid here and on an actual resize, never during navigation.
  RenderTexture2D transition_from_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  RenderTexture2D transition_to_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

  // Interactive content zoom (see render/content_zoom.h) — reuses the same
  // "render the slide into a texture, then draw that texture" approach as
  // the transition buffers above, for the same reason: it lets the zoomed
  // view be drawn with a single scaled DrawTexturePro instead of re-running
  // the whole text layout at a different scale.
  ContentZoomState zoom_state;
  RenderTexture2D zoom_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

  // true once ESC (with no zoom left to reset — see consume_escape_for_
  // content_zoom) or the window's close button has requested an exit.
  bool should_quit = false;

  // "Overview" mode (clickable grid of thumbnails, see
  // render/slide_overview.h) — activated while Ctrl is held down (see
  // navigation below). --force-overview starts the presentation already in
  // this mode, with all slides visible at once, instead of opening directly
  // on the initial slide — useful for whoever wants to choose where to
  // start before presenting.
  SlideOverviewState overview_state;
  if (args->force_overview) {
    overview_state.active = true;
    overview_state.focused_index = current_slide;
    rebuild_overview_thumbnails(overview_state, deck, renderer, config, image_cache);
  }

  // Layout of slide `slide_index` at the current window size — used both
  // by the "normal" relayout (resize/hot-reload) and to capture both sides
  // of a transition (see navigation below).
  auto compute_layout_for_slide = [&](int slide_index) {
    int window_width = GetScreenWidth();
    int window_height = GetScreenHeight();

    float initial_font_size = config.default_font_size *
                               (static_cast<float>(window_height) / static_cast<float>(config.default_window_height));

    float available_width = static_cast<float>(window_width) - 2.0f * config.margin_x;
    float available_height = static_cast<float>(window_height) - 2.0f * config.margin_y;

    return compute_fitted_layout(deck.slides[slide_index].content, renderer.fonts, config, initial_font_size,
                                  available_width, available_height, image_cache);
  };

  // Renders a slide that already has its layout computed into `target`
  // (one of the two transition buffers above) — used to capture both
  // sides ("from"/"to") of a transition (see render/slide_transition.h),
  // without needing to redraw the content on every frame of the animation.
  auto render_slide_into_texture = [&](RenderTexture2D target, const TextLayoutResult& slide_layout,
                                        const SlideParams& slide_params, int window_width, int window_height) {
    BeginTextureMode(target);
    ClearBackground(slide_params.bg_color.value_or(config.background_color));
    draw_centered_text(renderer, slide_layout, window_width, window_height, config, slide_params);
    EndTextureMode();
  };

  while (!WindowShouldClose() && !should_quit) {
    // --- Update ---
    if (poll_file_watch(watch_state, GetTime())) {
      std::optional<Document> reloaded = load_document(args->source_path);
      if (reloaded) {
        document = reloaded;
        deck = build_slide_deck(document->text, base_dir);
        ensure_styles_loaded(renderer, deck.usage);
        ensure_extra_fonts_loaded(renderer, deck.codepoints);
        current_slide = std::min(current_slide, static_cast<int>(deck.slides.size()) - 1);
        needs_relayout = true;
        reset_content_zoom(zoom_state);  // content changed under it — start clean

        // The content changed — the grid's thumbnails no longer match it.
        // If the grid is open right now, rebuild immediately (otherwise the
        // user would see stale thumbnails); otherwise just mark it dirty and
        // leave it to rebuild the next time the grid opens.
        overview_state.thumbnails_dirty = true;
        overview_state.focused_index =
            std::min(overview_state.focused_index, static_cast<int>(deck.slides.size()) - 1);
        if (overview_state.active) {
          rebuild_overview_thumbnails(overview_state, deck, renderer, config, image_cache);
        }
      }
    }

    if (IsWindowResized()) {
      needs_relayout = true;
      // The transition buffers and the zoom buffer all need to match the
      // window size (see update_and_draw_transition / draw_zoomed_content)
      // — recreate them at the new size.
      UnloadRenderTexture(transition_from_buffer);
      UnloadRenderTexture(transition_to_buffer);
      UnloadRenderTexture(zoom_buffer);
      transition_from_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
      transition_to_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
      zoom_buffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
      reset_content_zoom(zoom_state);  // simpler than re-deriving a sensible pivot/pan at the new size
    }

    // A resize or hot-reload in the middle of a transition invalidates what
    // was captured (size or content changed) — cancel instead of trying
    // to keep animating something stale.
    if (needs_relayout && transition_state.active) {
      cancel_transition(transition_state);
    }

    // "Overview" mode: holding Ctrl (either side) opens the thumbnail
    // grid; while active, mouse/arrows/wheel control the focus (see
    // update_slide_overview) instead of the normal navigation below.
    // Releasing Ctrl (both sides, see confirmed_by_ctrl_release) or clicking
    // a thumbnail confirms the focused slide and closes the grid — always
    // with an instant cut, without the destination slide's transition
    // effect (there's no natural "direction" between non-adjacent slides).
    // The !overview_state.active condition avoids reopening/resetting focus
    // and scroll if the grid was already active for another reason (e.g.
    // --force-overview, see below) and the user presses Ctrl for the first
    // time while inside it just to use the release-to-confirm gesture —
    // without this, the focus the user had already chosen with the
    // mouse/arrows would be discarded.
    if ((IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) && !overview_state.active) {
      overview_state.active = true;
      overview_state.focused_index = current_slide;
      overview_state.scroll_offset = 0.0f;
      reset_content_zoom(zoom_state);  // the grid shows every slide at once — zoom doesn't apply there
      if (overview_state.thumbnails_dirty) {
        rebuild_overview_thumbnails(overview_state, deck, renderer, config, image_cache);
      }
    }

    // Content zoom (see render/content_zoom.h) only makes sense on the
    // "real" slide — skipped while the overview grid is open (its own
    // wheel/click handling takes over, see update_slide_overview) or while
    // a transition is animating between two slides.
    if (!overview_state.active && !transition_state.active) {
      update_content_zoom(zoom_state, GetScreenWidth(), GetScreenHeight());
    }

    // ESC: first closes the zoom if the content is currently zoomed (see
    // consume_escape_for_content_zoom), otherwise it's the usual "close the
    // window" request — SetExitKey(KEY_NULL) above disabled raylib's own
    // handling of this key so this is the only place ESC is acted on.
    if (IsKeyPressed(KEY_ESCAPE) && !consume_escape_for_content_zoom(zoom_state)) {
      should_quit = true;
    }

    if (overview_state.active) {
      update_slide_overview(overview_state, deck, GetScreenWidth(), GetScreenHeight());

      bool confirmed_by_click = overview_consume_click(overview_state, GetScreenWidth(), GetScreenHeight(), deck);
      // Only confirms when NEITHER side of Ctrl is still pressed —
      // holding both down and releasing only one shouldn't close the
      // grid too early.
      bool confirmed_by_ctrl_release = (IsKeyReleased(KEY_LEFT_CONTROL) || IsKeyReleased(KEY_RIGHT_CONTROL)) &&
                                        !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL);

      if (confirmed_by_click || confirmed_by_ctrl_release) {
        current_slide = std::clamp(overview_state.focused_index, 0, static_cast<int>(deck.slides.size()) - 1);
        overview_state.active = false;
        needs_relayout = true;
      }
    }

    // Ignores navigation while a transition is playing — simpler than
    // interrupting/queuing, and the effect is short-lived (see
    // AppConfig::transition_duration_seconds). Also ignores it while the
    // thumbnail grid is active — in that case arrows control the grid's
    // focus, they don't advance/go back a slide underneath it.
    if (!transition_state.active && !overview_state.active) {
      int next_slide = -1;
      int direction = 0;
      if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) &&
          current_slide + 1 < static_cast<int>(deck.slides.size())) {
        next_slide = current_slide + 1;
        direction = 1;
      } else if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) && current_slide > 0) {
        next_slide = current_slide - 1;
        direction = -1;
      }

      if (next_slide != -1) {
        int window_width = GetScreenWidth();
        int window_height = GetScreenHeight();
        TransitionKind transition = deck.slides[next_slide].params.transition.value_or(args->default_transition);

        // Only captures the "from" side if it's actually going to animate —
        // saves two off-screen renders for nothing when the next slide
        // doesn't request a transition (transition=none, the usual
        // behavior: instant cut).
        if (transition != TransitionKind::None) {
          render_slide_into_texture(transition_from_buffer, layout, deck.slides[current_slide].params, window_width,
                                     window_height);
        }

        current_slide = next_slide;
        layout = compute_layout_for_slide(current_slide);
        needs_relayout = false;
        reset_content_zoom(zoom_state);  // zoom is a per-slide inspection tool, doesn't carry to the next one

        if (transition != TransitionKind::None) {
          const SlideParams& new_params = deck.slides[current_slide].params;
          render_slide_into_texture(transition_to_buffer, layout, new_params, window_width, window_height);
          Color to_background = new_params.bg_color.value_or(config.background_color);
          begin_transition(transition_state, transition, direction, to_background, GetTime());
        }
      }
    }

    if (needs_relayout) {
      layout = compute_layout_for_slide(current_slide);
      needs_relayout = false;
    }

    // Captures the current slide into zoom_buffer whenever it'll be needed
    // this frame — kept outside BeginDrawing/EndDrawing below, like the
    // transition buffers above, rather than nesting BeginTextureMode inside
    // the screen's own Begin/EndDrawing pair.
    if (!overview_state.active && !transition_state.active && is_content_zoomed(zoom_state)) {
      render_slide_into_texture(zoom_buffer, layout, deck.slides[current_slide].params, GetScreenWidth(),
                                 GetScreenHeight());
    }

    // --- Render ---
    BeginDrawing();
    if (overview_state.active) {
      // The grid's background uses the color of the slide currently being
      // shown (not a fixed generic color), so opening the grid doesn't
      // break visual continuity.
      Color current_bg = deck.slides[current_slide].params.bg_color.value_or(config.background_color);
      draw_slide_overview(overview_state, GetScreenWidth(), GetScreenHeight(), current_bg);
    } else if (transition_state.active) {
      update_and_draw_transition(transition_state, config, GetTime(), transition_from_buffer, transition_to_buffer);
    } else {
      const SlideParams& slide_params = deck.slides[current_slide].params;
      ClearBackground(slide_params.bg_color.value_or(config.background_color));
      if (is_content_zoomed(zoom_state)) {
        draw_zoomed_content(zoom_state, zoom_buffer, GetScreenWidth(), GetScreenHeight());
      } else {
        draw_centered_text(renderer, layout, GetScreenWidth(), GetScreenHeight(), config, slide_params);
      }
    }
    EndDrawing();

    // --screenshot: waits for things to settle, then exits, to inspect the
    // result without manual interaction (see cli/cli_args.h). Uses
    // LoadImageFromScreen + ExportImage instead of TakeScreenshot:
    // TakeScreenshot always concatenates the path with raylib's base
    // directory internally, breaking absolute paths.
    if (!args->screenshot_path.empty() && GetTime() - loop_entry_time >= args->screenshot_delay) {
      Image screen = LoadImageFromScreen();
      ExportImage(screen, args->screenshot_path.c_str());
      UnloadImage(screen);
      break;
    }
  }

  cancel_transition(transition_state);
  UnloadRenderTexture(transition_from_buffer);
  UnloadRenderTexture(transition_to_buffer);
  UnloadRenderTexture(zoom_buffer);
  unload_slide_overview(overview_state);
  unload_image_cache(image_cache);
  unload_text_renderer(renderer);
  CloseWindow();

  return 0;
}
