#include "render/content_zoom.h"

#include <algorithm>

namespace {

constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 4.0f;
constexpr float kWheelStep = 0.15f;  // per wheel notch
constexpr float kKeyStep = 0.25f;    // per +/- key press

constexpr double kDoubleClickSeconds = 0.35;
constexpr float kDoubleClickDistance = 12.0f;  // pixels

// A RenderTexture2D's `.texture` is stored bottom-to-top (OpenGL
// convention) — a negative height on the source rectangle undoes this when
// drawing with DrawTexturePro, otherwise the image comes out upside down.
// (Same fix as render/slide_transition.cpp's flipped_source.)
Rectangle flipped_source(const Texture2D& texture) {
  return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

// Keeps state.pan_offset within the range where the zoomed content still
// fully covers a window_width x window_height screen — beyond it, dragging
// further would start exposing background past the content's own edges
// (see draw_zoomed_content). At level 1.0 the valid range is exactly
// {0, 0}, so this also self-corrects any leftover offset as zoom eases back
// down toward normal, without needing a special case for it.
void clamp_pan(ContentZoomState& state, int window_width, int window_height) {
  float max_x = std::max(0.0f, static_cast<float>(window_width) * (state.level - 1.0f) / 2.0f);
  float max_y = std::max(0.0f, static_cast<float>(window_height) * (state.level - 1.0f) / 2.0f);
  state.pan_offset.x = std::clamp(state.pan_offset.x, -max_x, max_x);
  state.pan_offset.y = std::clamp(state.pan_offset.y, -max_y, max_y);
}

}  // namespace

bool is_content_zoomed(const ContentZoomState& state) {
  return state.level > kMinZoom;
}

void reset_content_zoom(ContentZoomState& state) {
  state.level = kMinZoom;
  state.pan_offset = Vector2{0.0f, 0.0f};
}

void update_content_zoom(ContentZoomState& state, int window_width, int window_height) {
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    state.level = std::clamp(state.level + wheel * kWheelStep, kMinZoom, kMaxZoom);
  }

  // '+' is Shift+'=' on the physical key most keyboards share with '='; the
  // numpad '+' (KEY_KP_ADD) needs no modifier to mean the same thing.
  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  if (IsKeyPressed(KEY_KP_ADD) || (IsKeyPressed(KEY_EQUAL) && shift)) {
    state.level = std::clamp(state.level + kKeyStep, kMinZoom, kMaxZoom);
  }
  if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) {
    state.level = std::clamp(state.level - kKeyStep, kMinZoom, kMaxZoom);
  }

  // '=' alone (no Shift) resets instead of zooming — it doesn't share the
  // step logic above, it clears the level (and any drag) outright.
  if (IsKeyPressed(KEY_EQUAL) && !shift) {
    reset_content_zoom(state);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 pos = GetMousePosition();

    double now = GetTime();
    float dx = pos.x - state.last_click_pos.x;
    float dy = pos.y - state.last_click_pos.y;
    bool is_double = (now - state.last_click_time) <= kDoubleClickSeconds &&
                      (dx * dx + dy * dy) <= (kDoubleClickDistance * kDoubleClickDistance);
    if (is_double) {
      reset_content_zoom(state);
      state.last_click_time = -1.0;  // a third click starts fresh, doesn't chain into another double
    } else {
      state.last_click_time = now;
      state.last_click_pos = pos;
    }

    // Anchors the drag at the press position, whether or not this turns
    // out to be a double-click — a reset zeroes pan_offset anyway, and
    // starting the anchor here means the very first frame of a drag (below)
    // computes a zero delta instead of jumping from a stale anchor.
    state.drag_anchor = pos;
  }

  // Only drags while actually zoomed in — at level 1.0 the content already
  // fills the screen exactly, there's nothing to pan.
  if (is_content_zoomed(state) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 pos = GetMousePosition();
    state.pan_offset.x += pos.x - state.drag_anchor.x;
    state.pan_offset.y += pos.y - state.drag_anchor.y;
    state.drag_anchor = pos;
  }

  clamp_pan(state, window_width, window_height);
}

bool consume_escape_for_content_zoom(ContentZoomState& state) {
  if (!is_content_zoomed(state)) {
    return false;
  }
  reset_content_zoom(state);
  return true;
}

void draw_zoomed_content(const ContentZoomState& state, RenderTexture2D source, int window_width,
                          int window_height) {
  Rectangle src = flipped_source(source.texture);
  float width = static_cast<float>(window_width) * state.level;
  float height = static_cast<float>(window_height) * state.level;
  Rectangle dest{static_cast<float>(window_width) / 2.0f + state.pan_offset.x,
                 static_cast<float>(window_height) / 2.0f + state.pan_offset.y, width, height};
  Vector2 origin{width / 2.0f, height / 2.0f};
  DrawTexturePro(source.texture, src, dest, origin, 0.0f, WHITE);
}
