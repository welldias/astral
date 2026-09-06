#include "slides/slide_params.h"

std::optional<TransitionKind> parse_transition_kind(const std::string &value) {
    if (value == "fade") {
        return TransitionKind::Fade;
    }
    if (value == "slide") {
        return TransitionKind::Slide;
    }
    if (value == "zoom") {
        return TransitionKind::Zoom;
    }
    if (value == "none") {
        return TransitionKind::None;
    }
    return std::nullopt;
}
