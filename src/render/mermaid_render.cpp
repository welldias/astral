#include "render/mermaid_render.h"

#include <nixie/nixie.h>

namespace {

nixie_theme_name_t map_theme(std::optional<ThemeKind> theme) {
    if (!theme) {
        return NIXIE_THEME_ZINC_DARK;
    }

    switch (*theme) {
    case ThemeKind::ZincLight:
        return NIXIE_THEME_ZINC_LIGHT;
    case ThemeKind::ZincDark:
        return NIXIE_THEME_ZINC_DARK;
    case ThemeKind::TokyoNight:
        return NIXIE_THEME_TOKYO_NIGHT;
    case ThemeKind::TokyoNightStorm:
        return NIXIE_THEME_TOKYO_NIGHT_STORM;
    case ThemeKind::TokyoNightLight:
        return NIXIE_THEME_TOKYO_NIGHT_LIGHT;
    case ThemeKind::CatppuccinMocha:
        return NIXIE_THEME_CATPPUCCIN_MOCHA;
    case ThemeKind::CatppuccinLatte:
        return NIXIE_THEME_CATPPUCCIN_LATTE;
    case ThemeKind::Nord:
        return NIXIE_THEME_NORD;
    case ThemeKind::NordLight:
        return NIXIE_THEME_NORD_LIGHT;
    case ThemeKind::Dracula:
        return NIXIE_THEME_DRACULA;
    case ThemeKind::GithubLight:
        return NIXIE_THEME_GITHUB_LIGHT;
    case ThemeKind::GithubDark:
        return NIXIE_THEME_GITHUB_DARK;
    case ThemeKind::SolarizedLight:
        return NIXIE_THEME_SOLARIZED_LIGHT;
    case ThemeKind::SolarizedDark:
        return NIXIE_THEME_SOLARIZED_DARK;
    case ThemeKind::OneDark:
        return NIXIE_THEME_SOLARIZED_DARK;
    case ThemeKind::CoffeeBean:
        return NIXIE_THEME_COFFEE_BEAN;
    }

    return NIXIE_THEME_ZINC_DARK;
}

} // namespace

const Texture2D *ensure_mermaid_image_loaded(ImageCache &cache, const std::string &mermaid_source, const std::string &font_path, std::optional<ThemeKind> theme) {
    auto loaded_it = cache.mermaid_loaded.find(mermaid_source);
    if (loaded_it != cache.mermaid_loaded.end()) {
        return &loaded_it->second;
    }
    if (cache.mermaid_failed.find(mermaid_source) != cache.mermaid_failed.end()) {
        return nullptr;
    }

    nixie_render_options_t opts = nixie_render_options_default();
    opts.transparent            = 0;
    opts.font_path              = font_path.c_str();
    opts.scale                  = 2.0;
    opts.theme                  = map_theme(theme);

    nixie_png_result_t result = nixie_render_png(mermaid_source.c_str(), &opts);
    if (result.error != NIXIE_OK) {
        cache.mermaid_failed[mermaid_source] = true;
        return nullptr;
    }

    Image image = LoadImageFromMemory(".png", result.data, static_cast<int>(result.size));
    nixie_free_png(result.data);

    if (image.data == nullptr) {
        cache.mermaid_failed[mermaid_source] = true;
        return nullptr;
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0) {
        cache.mermaid_failed[mermaid_source] = true;
        return nullptr;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    auto [inserted_it, inserted] = cache.mermaid_loaded.emplace(mermaid_source, texture);
    return &inserted_it->second;
}
