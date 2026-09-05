#pragma once

#include <string>
#include <vector>

#include "slides/slide_params.h"

struct SlideSource {
  std::string text;
  SlideParams params;
};

// Splits `source` into slides using the "$[...]" marker — a line whose
// first non-whitespace character is the marker's "$[" (the rest of the
// line, after the closing "]", is ignored). The marker line doesn't belong
// to any slide: the previous slide ends on the line above, the next one
// starts on the line below.
//
// Between the brackets go comma-separated parameters, in "key=value" format
// (whitespace around commas and '=' is ignored), which control the layout
// of the slide FOLLOWING the marker — not the slide that ends at it. An
// empty marker ("$[]") or a missing parameter is equivalent to using each
// parameter's default. A parameter with an unknown key or invalid value is
// ignored (keeps the default), so a typo doesn't break the slide split.
//
// Recognized parameters (see slides/slide_params.h::SlideParams):
//   align=left|center|right          — horizontal text alignment of the
//                                       following slide (default: center).
//   bg-color=<web color>              — background color of the following
//                                       slide (default: AppConfig::background_color).
//   text-color=<web color>            — text color of the following slide
//                                       (default: AppConfig::text_color).
//   block-color=<web color>           — code highlight color (inline and
//                                       block) of the following slide
//                                       (default: AppConfig::code_background_color).
//   transition=fade|slide|zoom|none  — transition effect when navigating to
//                                       the following slide, in either
//                                       direction (see render/slide_transition.h).
//                                       Without the parameter, uses the
//                                       command line's default (--transition,
//                                       see cli/cli_args.h — "none" if that
//                                       wasn't given either).
//
// Color values follow the web format: "#rgb", "#rgba", "#rrggbb" or
// "#rrggbbaa" (case-insensitive; hex digits). A value outside that format
// is treated as invalid and ignored, like any other malformed parameter.
//
// One or more markers before any content (for example, on the first line
// of the file) don't produce an empty slide: they only set the parameters
// of the first real slide, which starts on the first content line after
// them.
//
// With no marker at all, the whole document is a single slide (compatible
// with files that already existed before this feature), with the default
// parameters. Always returns at least one slide, even if empty.
std::vector<SlideSource> split_into_slides(const std::string& source);
