#pragma once

#include <optional>
#include <string>

#include "config/config.h"

// Built-in color themes the user can select with "--theme <name>" (see
// cli/cli_args.h). Each one overrides AppConfig::background_color,
// AppConfig::text_color and (usually) AppConfig::code_background_color —
// see apply_theme. Not configurable per slide: unlike SlideParams' own
// bg_color/text_color/block_color, a theme is a whole-presentation default.
enum class ThemeKind {
    ZincLight = 0,
    ZincDark,
    TokyoNight,
    TokyoNightStorm,
    TokyoNightLight,
    CatppuccinMocha,
    CatppuccinLatte,
    Nord,
    NordLight,
    Dracula,
    GithubLight,
    GithubDark,
    SolarizedLight,
    SolarizedDark,
    OneDark,
    CoffeeBean,
};

// Converts the value of the command line's "--theme" flag (see
// cli/cli_args.cpp) into a ThemeKind. nullopt if `value` doesn't match any
// of the names below (kebab-case, case-sensitive): "zinc-light",
// "zinc-dark", "tokyo-night", "tokyo-night-storm", "tokyo-night-light",
// "catppuccin-mocha", "catppuccin-latte", "nord", "nord-light", "dracula",
// "github-light", "github-dark", "solarized-light", "solarized-dark",
// "one-dark".
std::optional<ThemeKind> parse_theme_kind(const std::string &value);

// Overrides `config`'s colors with the ones defined by `theme`. Always sets
// background_color and text_color; code_background_color is left at
// whatever `config` already had for the one theme that doesn't define its
// own (ZincDark, which otherwise matches Astral's long-standing default
// colors) — see theme.cpp.
void apply_theme(AppConfig &config, ThemeKind theme);
