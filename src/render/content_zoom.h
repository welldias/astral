#pragma once

#include <raylib.h>

// State of the interactive content zoom (see main.cpp): lets the viewer
// magnify the current slide in place with the mouse wheel or the +/- keys,
// then drag it around with the mouse while zoomed in. Independent of
// TransitionKind::Zoom (the slide-to-slide transition effect in
// render/slide_transition.h) — this is a reading aid, not a transition.
struct ContentZoomState {
  // 1.0 = normal size (not zoomed). Never below 1.0 — there's nothing to
  // reveal by zooming out past the slide's normal, already-fitted size.
  float level = 1.0f;

  // How far the zoomed content has been dragged from centered, in screen
  // pixels at the CURRENT window size — see draw_zoomed_content. Clamped
  // every frame (update_content_zoom) so the content always still covers
  // the whole screen; always {0, 0} whenever level is back to 1.0.
  Vector2 pan_offset = {0.0f, 0.0f};

  // Left mouse button position last frame, while held down — the delta
  // between this and the current position is how far to drag pan_offset
  // this frame (see update_content_zoom). Reset on every new press so the
  // first frame of a drag doesn't jump.
  Vector2 drag_anchor = {0.0f, 0.0f};

  // Tracks the last left-click, to recognize a double-click in
  // update_content_zoom — raylib has no built-in double-click event.
  double last_click_time = -1.0;
  Vector2 last_click_pos = {0.0f, 0.0f};
};

// true if `state.level` is above normal — main.cpp uses this both to decide
// whether the render-texture indirection (draw_zoomed_content) is needed at
// all, and to give ESC its double meaning (see consume_escape_for_content_zoom).
bool is_content_zoomed(const ContentZoomState& state);

// Resets zoom and pan back to their defaults (level 1.0, no drag offset) —
// used both by consume_escape_for_content_zoom/update_content_zoom below and
// by main.cpp whenever the underlying slide changes out from under the view
// (navigating to another slide, a hot-reload, a resize, or opening the
// overview grid): those aren't "the user asked to reset zoom" moments, but
// carrying a stale zoom/pan into them doesn't make sense either.
void reset_content_zoom(ContentZoomState& state);

// Reads wheel/keyboard/mouse input and updates state.level and
// state.pan_offset. Call at most once per frame, and only while it's
// meaningful to change zoom — the caller skips this while the overview grid
// is open or a transition is playing (see main.cpp), so the wheel and
// clicks go to the right place instead of also zooming/panning underneath
// them. `window_width`/`window_height` are the current window size, needed
// to keep pan_offset clamped to its valid range at the current level (see
// draw_zoomed_content).
//
// Wheel and the numpad/'+'/'-' keys step the level up or down, clamped to
// [1.0, 4.0]. The bare '=' key (no Shift — '+' on most layouts is Shift+'='
// and is handled above as zoom-in, not here) and a double-click both reset
// straight to 1.0 (see reset_content_zoom). While level is above 1.0,
// holding the left mouse button and moving it drags the content — the view
// follows the cursor like grabbing a piece of paper, clamped so it always
// still covers the screen.
void update_content_zoom(ContentZoomState& state, int window_width, int window_height);

// ESC has two jobs in this app (see main.cpp): if the content is currently
// zoomed, the first press should only reset it back to normal — not close
// the window. Returns true if it consumed the press this way (state.level
// is back to 1.0 and state.pan_offset back to {0,0}); false means there was
// nothing to reset and the caller should fall back to its usual "close the
// window" handling.
bool consume_escape_for_content_zoom(ContentZoomState& state);

// Draws `source` (a RenderTexture2D already holding the current slide,
// rendered at exactly window_width x window_height) scaled up by
// state.level and shifted by state.pan_offset — magnifying and panning, not
// revealing anything beyond the slide's own edges: dragging just moves
// which part of the already-rendered slide is under the screen's window,
// clamped so the screen is always fully covered. Assumes BeginDrawing() was
// already called; draws directly to the screen, not to another texture.
void draw_zoomed_content(const ContentZoomState& state, RenderTexture2D source, int window_width,
                          int window_height);
