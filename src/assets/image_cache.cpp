#include "assets/image_cache.h"

#include <filesystem>

std::string resolve_image_path(const std::string &base_dir, const std::string &image_path) {
    if (image_path.empty()) {
        return image_path;
    }

    std::filesystem::path path(image_path);
    if (path.is_absolute()) {
        return image_path;
    }

    std::filesystem::path base = base_dir.empty() ? std::filesystem::current_path() : std::filesystem::path(base_dir);
    return (base / path).lexically_normal().string();
}

const Texture2D *ensure_image_loaded(ImageCache &cache, const std::string &resolved_path) {
    auto loaded_it = cache.loaded.find(resolved_path);
    if (loaded_it != cache.loaded.end()) {
        return &loaded_it->second;
    }
    if (cache.failed.find(resolved_path) != cache.failed.end()) {
        return nullptr;
    }

    if (resolved_path.empty() || !FileExists(resolved_path.c_str())) {
        cache.failed[resolved_path] = true;
        return nullptr;
    }

    Texture2D texture = LoadTexture(resolved_path.c_str());
    if (texture.id == 0) {
        cache.failed[resolved_path] = true;
        return nullptr;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    auto [inserted_it, inserted] = cache.loaded.emplace(resolved_path, texture);
    return &inserted_it->second;
}

void unload_image_cache(ImageCache &cache) {
    for (auto &[path, texture] : cache.loaded) {
        UnloadTexture(texture);
    }
    cache.loaded.clear();
    cache.failed.clear();

    for (auto &[source, texture] : cache.mermaid_loaded) {
        UnloadTexture(texture);
    }
    cache.mermaid_loaded.clear();
    cache.mermaid_failed.clear();
}
