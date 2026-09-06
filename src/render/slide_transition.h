#pragma once

#include <raylib.h>

#include "config/config.h"
#include "slides/slide_params.h"

// State of a slide transition in progress (see
// slides/slide_params.h::TransitionKind). Doesn't hold the source/
// destination slide textures — main.cpp keeps two persistent
// RenderTexture2D, reused across every transition (recreating a
// RenderTexture2D on every slide change already caused a short visible
// stutter/flicker right at the moment of the change; reusing the same
// two avoids the GPU allocation cost at that moment).
struct SlideTransitionState {
    bool active         = false;
    TransitionKind kind = TransitionKind::None;
    int direction       = 1; // +1 forward (Right/Down), -1 backward (Left/Up)
    double start_time   = 0.0;

    // Color to clear the screen behind the composition — needed for Zoom,
    // which exposes a border around the destination slide while it's still
    // smaller than 100%. Uses the background color of the DESTINATION
    // slide (more natural: that's where the transition is heading).
    Color to_background = BLACK;
};

// Starts a transition — doesn't touch any texture, just the state (see
// update_and_draw_transition, which receives from/to on every call).
// Doesn't check whether `state` was already active; the caller must have
// called cancel_transition beforehand, if applicable.
void begin_transition(SlideTransitionState &state, TransitionKind kind, int direction, Color to_background, double now);

// Cancels a transition in progress — used when a window resize or a file
// hot-reload interrupts the animation (see main.cpp). Does nothing if
// state.active is already false. Doesn't free any texture: the two
// RenderTexture2D reused across transitions belong to main.cpp, which
// decides when to (re)allocate them, e.g. on an actual resize.
void cancel_transition(SlideTransitionState &state);

// Draws the current frame of the transition — assumes BeginDrawing() has
// already been called by whoever calls this function, and draws directly
// to the screen (not to another RenderTexture). `from`/`to` must have
// exactly the window's size (see main.cpp) — the same pair used across
// every call of a given transition. Advances progress based on `now`
// (same time source as GetTime()) and config.transition_duration_seconds.
//
// Returns true if the transition is still active (the caller must not do
// the normal slide drawing this frame) or false if it just finished right
// now (state.active is already false; the last frame, at t=1, was already
// drawn by this call before returning) — the caller should fall through
// to the normal drawing of the current slide in this same frame.
bool update_and_draw_transition(SlideTransitionState &state, const AppConfig &config, double now, RenderTexture2D from, RenderTexture2D to);
