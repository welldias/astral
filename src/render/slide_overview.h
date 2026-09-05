#pragma once

#include <raylib.h>

#include <vector>

#include "assets/image_cache.h"
#include "config/config.h"
#include "render/text_renderer.h"
#include "slides/slide_deck.h"

// State of "overview" mode (thumbnail grid, see main.cpp): activated while
// Ctrl (left or right) is held down; on release (or when clicking a
// thumbnail), the focused slide becomes the current_slide.
struct SlideOverviewState {
  bool active = false;

  // One RenderTexture2D per slide, at a fixed reduced resolution (see
  // kThumbnailWidth/kThumbnailHeight in slide_overview.cpp) — rendered
  // with the same compute_fitted_layout + draw_centered_text logic used
  // for the "real" slide, just onto a small, fixed target (doesn't scale
  // with the window size). Empty until the first rebuild.
  std::vector<RenderTexture2D> thumbnails;

  // true: `thumbnails` no longer matches the current content (the document
  // hasn't been rendered even once yet, or it changed via hot-reload) —
  // see rebuild_overview_thumbnails.
  bool thumbnails_dirty = true;

  int focused_index = 0;
  float scroll_offset = 0.0f;  // grid's vertical offset, in pixels
};

// (Re)builds `state.thumbnails`: releases the old textures (if any) and
// renders a new RenderTexture2D per slide in `deck`. Call when the grid
// opens with thumbnails_dirty == true, and immediately (even with the grid
// already open) if a hot-reload happens while state.active is true — so
// stale thumbnails aren't left visible on screen.
void rebuild_overview_thumbnails(SlideOverviewState& state, const SlideDeck& deck, const TextRenderer& renderer,
                                  const AppConfig& config, ImageCache& image_cache);

// Updates focus/scroll from mouse and keyboard — call on every frame where
// state.active is true, before drawing. window_width/window_height are the
// current window size (the grid recalculates columns/positions from these
// every frame, it doesn't keep its own layout — only the textures
// themselves are fixed).
void update_slide_overview(SlideOverviewState& state, const SlideDeck& deck, int window_width, int window_height);

// true if the mouse's left click, this frame, landed on a thumbnail — in
// that case state.focused_index has already been updated to it. Called by
// main.cpp to decide whether to close the grid and confirm right away, in
// addition to the normal confirmation on releasing Ctrl.
bool overview_consume_click(SlideOverviewState& state, int window_width, int window_height, const SlideDeck& deck);

// Draws the grid (background `background`, each thumbnail, highlight on
// the focused one) — assumes BeginDrawing() was already called; draws to
// the screen, not a RenderTexture. `background` is the background color of
// the slide currently being shown (see main.cpp), so opening the grid
// doesn't break visual continuity.
void draw_slide_overview(const SlideOverviewState& state, int window_width, int window_height, Color background);

void unload_slide_overview(SlideOverviewState& state);
