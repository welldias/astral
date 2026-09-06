#pragma once

#include <optional>
#include <string>

#include <raylib.h>

#include "text/text_align.h"

// Transition effect used when arriving at a slide (see
// SlideParams::transition and render/slide_transition.h). None: instant
// cut, the usual behavior.
enum class TransitionKind {
    None,
    Fade,
    Slide,
    Zoom,
};

// Converts the value of a "transition=..." parameter — used both in a
// slide's "$[...]" marker (see slides/slide_splitter.cpp) and in the
// command line's "--transition" (see cli/cli_args.cpp) — into a
// TransitionKind. nullopt if `value` isn't "fade", "slide", "zoom" or
// "none".
std::optional<TransitionKind> parse_transition_kind(const std::string &value);

// Layout parameters of a slide, read from the marker that precedes it (see
// slides/slide_splitter.h). Each field has a default value that reproduces
// the behavior of when the corresponding parameter isn't given — missing
// colors (nullopt) fall back to the corresponding AppConfig value (see
// render/text_renderer.h::draw_centered_text and main.cpp).
struct SlideParams {
    TextAlign align = TextAlign::Center;

    // Colors in web format: "#rgb", "#rgba", "#rrggbb" or "#rrggbbaa" (see
    // the parser in slide_splitter.cpp). nullopt: uses AppConfig's color.
    std::optional<Color> bg_color;    // slide background (AppConfig::background_color)
    std::optional<Color> text_color;  // text (AppConfig::text_color)
    std::optional<Color> block_color; // code highlight, inline and block (AppConfig::code_background_color)

    // Transition effect used when NAVIGATING TO this slide (in either
    // direction — see render/slide_transition.h). nullopt: the slide didn't
    // give a "transition" parameter in its marker — falls back to
    // CliArgs::default_transition (see main.cpp), not necessarily None; a
    // slide that really wants no transition, despite the command line's
    // default, states "transition=none" explicitly.
    std::optional<TransitionKind> transition;
};
