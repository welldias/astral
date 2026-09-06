#include "config/theme.h"

#include <optional>

namespace {

// One entry per ThemeKind. code_background is nullopt only for ZincDark,
// whose background/text already match AppConfig's own defaults closely
// enough that it just reuses the default code_background_color too (see
// apply_theme).
struct ThemeColors {
    Color background;
    Color text;
    Color code_background;
} static const theme_list[] = {
    // ZincLight
    {
     Color{ 255, 255, 255, 255 },
     Color{ 39, 39, 42, 255 },
     Color{ 209, 217, 224, 255 },
     },
    // ZincDark
    {
     Color{ 24, 24, 27, 255 },
     Color{ 250, 250, 250, 255 },
     Color{ 75, 82, 99, 255 },
     },
    // TokyoNight
    {
     Color{ 26, 27, 38, 255 },
     Color{ 169, 177, 214, 255 },
     Color{ 61, 89, 161, 255 },
     },
    // TokyoNightStorm
    {
     Color{ 36, 40, 59, 255 },
     Color{ 169, 177, 214, 255 },
     Color{ 61, 89, 161, 255 },
     },
    // TokyoNightLight
    {
     Color{ 213, 214, 219, 255 },
     Color{ 52, 59, 88, 255 },
     Color{ 52, 84, 138, 255 },
     },
    // CatppuccinMocha
    {
     Color{ 30, 30, 46, 255 },
     Color{ 205, 214, 244, 255 },
     Color{ 88, 91, 112, 255 },
     },
    // CatppuccinLatte
    {
     Color{ 239, 241, 245, 255 },
     Color{ 76, 79, 105, 255 },
     Color{ 156, 160, 176, 255 },
     },
    // Nord
    {
     Color{ 46, 52, 64, 255 },
     Color{ 216, 222, 233, 255 },
     Color{ 76, 86, 106, 255 },
     },
    // NordLight
    {
     Color{ 236, 239, 244, 255 },
     Color{ 46, 52, 64, 255 },
     Color{ 170, 177, 192, 255 },
     },
    // Dracula
    {
     Color{ 40, 42, 54, 255 },
     Color{ 248, 248, 242, 255 },
     Color{ 98, 114, 164, 255 },
     },
    // GithubLight
    {
     Color{ 255, 255, 255, 255 },
     Color{ 31, 35, 40, 255 },
     Color{ 209, 217, 224, 255 },
     },
    // GithubDark
    {
     Color{ 13, 17, 23, 255 },
     Color{ 230, 237, 243, 255 },
     Color{ 61, 68, 77, 255 },
     },
    // SolarizedLight
    {
     Color{ 253, 246, 227, 255 },
     Color{ 101, 123, 131, 255 },
     Color{ 147, 161, 161, 255 },
     },
    // SolarizedDarkWWW
    {
     Color{ 0, 43, 54, 255 },
     Color{ 131, 148, 150, 255 },
     Color{ 88, 110, 117, 255 },
     },
    // OneDark
    {
     Color{ 40, 44, 52, 255 },
     Color{ 171, 178, 191, 255 },
     Color{ 75, 82, 99, 255 },
     },
    // CoffeeBean
    {
     Color{ 243, 233, 220, 255 },
     Color{ 94, 48, 35, 255 },
     Color{ 238, 197, 160, 255 },
     },
};

const ThemeColors &theme_colors(ThemeKind theme) {
    return theme_list[static_cast<int>(theme)];
}

} // namespace

std::optional<ThemeKind> parse_theme_kind(const std::string &value) {
    if (value == "zinc-light")
        return ThemeKind::ZincLight;
    if (value == "zinc-dark")
        return ThemeKind::ZincDark;
    if (value == "tokyo-night")
        return ThemeKind::TokyoNight;
    if (value == "tokyo-night-storm")
        return ThemeKind::TokyoNightStorm;
    if (value == "tokyo-night-light")
        return ThemeKind::TokyoNightLight;
    if (value == "catppuccin-mocha")
        return ThemeKind::CatppuccinMocha;
    if (value == "catppuccin-latte")
        return ThemeKind::CatppuccinLatte;
    if (value == "nord")
        return ThemeKind::Nord;
    if (value == "nord-light")
        return ThemeKind::NordLight;
    if (value == "dracula")
        return ThemeKind::Dracula;
    if (value == "github-light")
        return ThemeKind::GithubLight;
    if (value == "github-dark")
        return ThemeKind::GithubDark;
    if (value == "solarized-light")
        return ThemeKind::SolarizedLight;
    if (value == "solarized-dark")
        return ThemeKind::SolarizedDark;
    if (value == "one-dark")
        return ThemeKind::OneDark;
    if (value == "coffee-bean")
        return ThemeKind::CoffeeBean;
    return std::nullopt;
}

void apply_theme(AppConfig &config, ThemeKind theme) {
    const ThemeColors &colors = theme_colors(theme);

    config.background_color      = colors.background;
    config.text_color            = colors.text;
    config.code_background_color = colors.code_background;
}
