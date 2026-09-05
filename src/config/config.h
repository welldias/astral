#pragma once

#include <raylib.h>

#include <string>

struct AppConfig {
  int default_window_width;
  int default_window_height;
  int min_window_width;
  int min_window_height;
  std::string window_title;

  // "font_size = default_font_size * (current_window_height / default_window_height)"
  float default_font_size;
  float min_font_size;
  float max_font_size;

  // Bake resolution of the SDF atlas; since it's SDF, text stays sharp
  // both above and below this base size.
  float font_atlas_base_size;

  float line_height_multiplier;
  float margin_x;
  float margin_y;

  // Extra space between consecutive content blocks (paragraphs/headings),
  // on top of each one's line_height. Scales together with the font on
  // auto-shrink.
  float paragraph_spacing;

  // Space between a thematic break (---) and the text above it — smaller
  // than paragraph_spacing, so the break sits closer to the text above.
  // The space below the break still uses paragraph_spacing normally.
  float horizontal_rule_top_spacing;

  // Size of each heading level, as a multiple of default_font_size.
  // Headings of level >4 use heading_scale_h4 (see parse_markdown).
  float heading_scale_h1;
  float heading_scale_h2;
  float heading_scale_h3;
  float heading_scale_h4;

  // Size of blockquote text (BlockKind::BlockQuote), as a multiple of
  // default_font_size — smaller than normal text (1.0).
  float blockquote_scale;

  // Duration of any transition effect between slides (see
  // slides/slide_params.h::TransitionKind and render/slide_transition.h)
  // — shared by all three effects, not configurable per slide.
  float transition_duration_seconds;

  Color background_color;
  Color text_color;

  // Background of the code highlight (inline `code` and ```code``` blocks)
  // — a lighter gray than background_color.
  Color code_background_color;

  // Used only if discovering the operating system's default font fails.
  std::string bundled_font_path;
};

AppConfig make_default_config();
