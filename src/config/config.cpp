#include "config/config.h"

AppConfig make_default_config() {
  AppConfig config;

  config.default_window_width = 1280;
  config.default_window_height = 720;
  config.min_window_width = 480;
  config.min_window_height = 270;
  config.window_title = "Astral";

  config.default_font_size = 32.0f;
  config.min_font_size = 12.0f;
  config.max_font_size = 200.0f;
  config.font_atlas_base_size = 128.0f;

  config.line_height_multiplier = 1.4f;
  config.margin_x = 80.0f;
  config.margin_y = 60.0f;

  // Equal to a "blank line" at the default size, to reproduce the
  // paragraph spacing that plain-text mode already had.
  config.paragraph_spacing = config.default_font_size * config.line_height_multiplier;

  // A fraction of the normal spacing, so the thematic break sits
  // visibly closer to the text above it.
  config.horizontal_rule_top_spacing = config.paragraph_spacing * 0.01f;

  config.heading_scale_h1 = 2.00f;
  config.heading_scale_h2 = 1.60f;
  config.heading_scale_h3 = 1.20f;
  config.heading_scale_h4 = 1.00f;

  config.blockquote_scale = 0.70f;

  config.transition_duration_seconds = 0.35f;

  config.background_color = Color{18, 18, 20, 255};
  config.text_color = Color{235, 235, 235, 255};
  config.code_background_color = Color{54, 54, 58, 255};

  config.bundled_font_path = "assets/fonts/default.ttf";

  return config;
}
