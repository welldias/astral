#include "cli/cli_args.h"

#include <cstdlib>

namespace {
// Default wait time after the window opens before capturing the
// screenshot: gives the swap chain buffer time to stabilize — without
// it, the first frame can be captured blank/black on some
// compositors.
constexpr double kDefaultScreenshotDelay = 0.5;
} // namespace

std::optional<CliArgs> parse_cli_args(int argc, char **argv) {
    CliArgs args;
    args.screenshot_delay   = kDefaultScreenshotDelay;
    args.initial_slide      = 1;
    args.default_transition = TransitionKind::None;

    bool has_source = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--screenshot") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.screenshot_path = argv[++i];
        } else if (arg == "--screenshot-delay") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.screenshot_delay = std::atof(argv[++i]);
        } else if (arg == "--slide") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.initial_slide = std::atoi(argv[++i]);
        } else if (arg == "--transition") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            std::optional<TransitionKind> transition = parse_transition_kind(argv[++i]);
            if (!transition) {
                return std::nullopt; // unrecognized value: usage error (unlike the $[...] marker, which ignores it)
            }
            args.default_transition = *transition;
        } else if (arg == "--emoji-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.emoji_font_path = argv[++i];
        } else if (arg == "--asian-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.asian_font_path = argv[++i];
        } else if (arg == "--regular-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.regular_font_path = argv[++i];
        } else if (arg == "--italic-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.italic_font_path = argv[++i];
        } else if (arg == "--bold-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.bold_font_path = argv[++i];
        } else if (arg == "--mono-font") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            args.mono_font_path = argv[++i];
        } else if (arg == "--force-overview") {
            args.force_overview = true;
        } else if (arg == "--theme") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            std::optional<ThemeKind> theme = parse_theme_kind(argv[++i]);
            if (!theme) {
                return std::nullopt; // unrecognized value: usage error
            }
            args.theme = theme;
        } else if (!has_source) {
            args.source_path = arg;
            has_source       = true;
        } else {
            return std::nullopt; // extra positional argument or unknown flag
        }
    }

    if (!has_source) {
        return std::nullopt;
    }

    return args;
}
