#include "render/slide_transition.h"

#include <algorithm>

namespace {

// Smoothstep: same smooth acceleration/deceleration curve across all
// three effects, instead of linear progress (which looks mechanical).
float ease_in_out(float t) {
  return t * t * (3.0f - 2.0f * t);
}

// A RenderTexture2D's `.texture` is stored bottom-to-top (OpenGL
// convention) — a negative height on the source rectangle undoes this
// when drawing with DrawTexturePro, otherwise the image comes out upside
// down.
Rectangle flipped_source(const Texture2D& texture) {
  return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

}  // namespace

void begin_transition(SlideTransitionState& state, TransitionKind kind, int direction, Color to_background,
                       double now) {
  state.active = true;
  state.kind = kind;
  state.direction = direction;
  state.start_time = now;
  state.to_background = to_background;
}

void cancel_transition(SlideTransitionState& state) {
  state.active = false;
}

bool update_and_draw_transition(SlideTransitionState& state, const AppConfig& config, double now,
                                 RenderTexture2D from, RenderTexture2D to) {
  float t = static_cast<float>((now - state.start_time) / config.transition_duration_seconds);
  t = std::clamp(t, 0.0f, 1.0f);
  float eased = ease_in_out(t);

  float window_width = static_cast<float>(to.texture.width);
  float window_height = static_cast<float>(to.texture.height);

  ClearBackground(state.to_background);

  Rectangle from_source = flipped_source(from.texture);
  Rectangle to_source = flipped_source(to.texture);

  switch (state.kind) {
    case TransitionKind::Fade: {
      DrawTexturePro(from.texture, from_source, Rectangle{0.0f, 0.0f, window_width, window_height},
                      Vector2{0.0f, 0.0f}, 0.0f, WHITE);
      auto alpha = static_cast<unsigned char>(eased * 255.0f);
      DrawTexturePro(to.texture, to_source, Rectangle{0.0f, 0.0f, window_width, window_height}, Vector2{0.0f, 0.0f},
                      0.0f, Color{255, 255, 255, alpha});
      break;
    }
    case TransitionKind::Slide: {
      float offset = eased * window_width * static_cast<float>(state.direction);
      DrawTexturePro(from.texture, from_source, Rectangle{-offset, 0.0f, window_width, window_height},
                      Vector2{0.0f, 0.0f}, 0.0f, WHITE);
      float to_x = static_cast<float>(state.direction) * window_width - offset;
      DrawTexturePro(to.texture, to_source, Rectangle{to_x, 0.0f, window_width, window_height}, Vector2{0.0f, 0.0f},
                      0.0f, WHITE);
      break;
    }
    case TransitionKind::Zoom: {
      auto from_alpha = static_cast<unsigned char>((1.0f - eased) * 255.0f);
      DrawTexturePro(from.texture, from_source, Rectangle{0.0f, 0.0f, window_width, window_height},
                      Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, from_alpha});

      float scale = 0.85f + 0.15f * eased;
      float scaled_width = window_width * scale;
      float scaled_height = window_height * scale;
      float to_x = (window_width - scaled_width) * 0.5f;
      float to_y = (window_height - scaled_height) * 0.5f;
      auto to_alpha = static_cast<unsigned char>(eased * 255.0f);
      DrawTexturePro(to.texture, to_source, Rectangle{to_x, to_y, scaled_width, scaled_height}, Vector2{0.0f, 0.0f},
                      0.0f, Color{255, 255, 255, to_alpha});
      break;
    }
    case TransitionKind::None:
      break;
  }

  if (t >= 1.0f) {
    cancel_transition(state);
    return false;
  }
  return true;
}
