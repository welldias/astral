#include "render/slide_overview.h"

#include <algorithm>
#include <cmath>

namespace {

// Fixed resolution of each thumbnail — doesn't scale with the window size
// (otherwise every resize would force rebuilding all the textures). Keeps
// the 16:9 ratio of AppConfig's default window size.
constexpr int kThumbnailWidth  = 427; // config.default_window_width / 3
constexpr int kThumbnailHeight = 240; // config.default_window_height / 3

constexpr float kCellPadding          = 16.0f; // breathing room between cells and at the grid's edges
constexpr float kFocusBorderThickness = 4.0f;
constexpr float kScrollSpeed          = 60.0f; // pixels per mouse wheel "click"

// A RenderTexture2D's `.texture` is stored bottom-up (OpenGL convention) —
// a negative height on the source rectangle undoes that when drawing with
// DrawTexturePro, otherwise the thumbnail comes out upside down (same
// pattern as render/slide_transition.cpp::flipped_source).
Rectangle flipped_source(const Texture2D &texture) {
    return Rectangle{ 0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height) };
}

// Opposite color of `color` by inverting RGB (channel by channel,
// "255 - value") — used for the focus highlight border (see
// draw_slide_overview) so it never ends up looking too close to the
// grid's background, which changes from slide to slide (bg-color from the
// "$[...]" marker). Alpha preserved.
Color invert_color(Color color) {
    return Color{ static_cast<unsigned char>(255 - color.r), static_cast<unsigned char>(255 - color.g), static_cast<unsigned char>(255 - color.b), color.a };
}

int compute_columns(int window_width) {
    int columns = static_cast<int>((static_cast<float>(window_width) + kCellPadding) / (static_cast<float>(kThumbnailWidth) + kCellPadding));
    return std::max(columns, 1);
}

int compute_rows(int slide_count, int columns) {
    if (slide_count <= 0) {
        return 0;
    }
    return (slide_count + columns - 1) / columns;
}

Rectangle thumbnail_cell_rect(int index, int columns, int window_width, float scroll_offset) {
    int col = index % columns;
    int row = index / columns;

    float grid_width = static_cast<float>(columns) * (kThumbnailWidth + kCellPadding) - kCellPadding;
    float start_x    = (static_cast<float>(window_width) - grid_width) / 2.0f;

    float x = start_x + static_cast<float>(col) * (kThumbnailWidth + kCellPadding);
    float y = kCellPadding + static_cast<float>(row) * (kThumbnailHeight + kCellPadding) - scroll_offset;

    return Rectangle{ x, y, static_cast<float>(kThumbnailWidth), static_cast<float>(kThumbnailHeight) };
}

int hit_test_thumbnail(Vector2 mouse, int slide_count, int columns, int window_width, float scroll_offset) {
    for (int i = 0; i < slide_count; ++i) {
        if (CheckCollisionPointRec(mouse, thumbnail_cell_rect(i, columns, window_width, scroll_offset))) {
            return i;
        }
    }
    return -1;
}

// Largest valid scroll_offset — grid content flush with the bottom of the
// window, never leaving blank space at the bottom while there's still more
// blank space available above (scrolling "capped" at both ends, like any
// list).
float max_scroll_offset(int slide_count, int columns, int window_height) {
    int rows             = compute_rows(slide_count, columns);
    float content_height = kCellPadding + static_cast<float>(rows) * (kThumbnailHeight + kCellPadding);
    return std::max(0.0f, content_height - static_cast<float>(window_height));
}

void unload_thumbnail_textures(SlideOverviewState &state) {
    for (RenderTexture2D &texture : state.thumbnails) {
        UnloadRenderTexture(texture);
    }
    state.thumbnails.clear();
}

} // namespace

void rebuild_overview_thumbnails(SlideOverviewState &state, const SlideDeck &deck, const TextRenderer &renderer, const AppConfig &config, ImageCache &image_cache) {
    unload_thumbnail_textures(state);
    state.thumbnails.reserve(deck.slides.size());

    // config.margin_x/y are absolute pixel values calibrated for
    // default_window_width/height — applied without scaling, they'd eat a
    // disproportionately larger fraction of the thumbnail's much smaller
    // width/height. Scaling by the same ratio as the font size replicates
    // the real slide's margin/height proportion (see compute_layout_for_slide
    // in main.cpp), making the thumbnail an actually faithful scaled-down
    // version, not one with disproportionate margins.
    float scale             = static_cast<float>(kThumbnailHeight) / static_cast<float>(config.default_window_height);
    float initial_font_size = config.default_font_size * scale;
    float available_width   = static_cast<float>(kThumbnailWidth) - 2.0f * config.margin_x * scale;
    float available_height  = static_cast<float>(kThumbnailHeight) - 2.0f * config.margin_y * scale;

    for (const Slide &slide : deck.slides) {
        TextLayoutResult layout = compute_fitted_layout(slide.content, renderer.fonts, config, initial_font_size, available_width, available_height, image_cache);

        RenderTexture2D target = LoadRenderTexture(kThumbnailWidth, kThumbnailHeight);
        BeginTextureMode(target);
        ClearBackground(slide.params.bg_color.value_or(config.background_color));
        draw_centered_text(renderer, layout, kThumbnailWidth, kThumbnailHeight, config, slide.params);
        EndTextureMode();

        state.thumbnails.push_back(target);
    }

    state.thumbnails_dirty = false;
}

void update_slide_overview(SlideOverviewState &state, const SlideDeck &deck, int window_width, int window_height) {
    int slide_count = static_cast<int>(deck.slides.size());
    if (slide_count == 0) {
        return;
    }
    state.focused_index = std::clamp(state.focused_index, 0, slide_count - 1);

    int columns      = compute_columns(window_width);
    float max_scroll = max_scroll_offset(slide_count, columns, window_height);

    // Arrows move the focus by row/column — clamped to the total number of
    // slides (doesn't "wrap around" from one end to the other).
    if (IsKeyPressed(KEY_RIGHT)) {
        state.focused_index = std::min(state.focused_index + 1, slide_count - 1);
    } else if (IsKeyPressed(KEY_LEFT)) {
        state.focused_index = std::max(state.focused_index - 1, 0);
    } else if (IsKeyPressed(KEY_DOWN)) {
        state.focused_index = std::min(state.focused_index + columns, slide_count - 1);
    } else if (IsKeyPressed(KEY_UP)) {
        state.focused_index = std::max(state.focused_index - columns, 0);
    }

    // Hover: moving the mouse over a thumbnail focuses it — mouse outside the
    // grid doesn't "steal" the focus back (keeps whatever the last
    // interaction set).
    int hovered = hit_test_thumbnail(GetMousePosition(), slide_count, columns, window_width, state.scroll_offset);
    if (hovered != -1) {
        state.focused_index = hovered;
    }

    // Mouse wheel scrolls the grid vertically.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        state.scroll_offset -= wheel * kScrollSpeed;
    }

    // Auto-scroll: keeps the focused cell visible (arrows can move the focus
    // outside the currently visible area).
    int row           = state.focused_index / columns;
    float cell_top    = kCellPadding + static_cast<float>(row) * (kThumbnailHeight + kCellPadding);
    float cell_bottom = cell_top + kThumbnailHeight;
    if (cell_top - state.scroll_offset < 0.0f) {
        state.scroll_offset = cell_top - kCellPadding;
    } else if (cell_bottom - state.scroll_offset > static_cast<float>(window_height)) {
        state.scroll_offset = cell_bottom - static_cast<float>(window_height) + kCellPadding;
    }

    state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll);
}

bool overview_consume_click(SlideOverviewState &state, int window_width, int window_height, const SlideDeck &deck) {
    (void)window_height;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return false;
    }

    int slide_count = static_cast<int>(deck.slides.size());
    int columns     = compute_columns(window_width);
    int hit         = hit_test_thumbnail(GetMousePosition(), slide_count, columns, window_width, state.scroll_offset);
    if (hit == -1) {
        return false;
    }

    state.focused_index = hit;
    return true;
}

void draw_slide_overview(const SlideOverviewState &state, int window_width, int window_height, Color background) {
    ClearBackground(background);

    int slide_count = static_cast<int>(state.thumbnails.size());
    if (slide_count == 0) {
        return;
    }
    int columns = compute_columns(window_width);

    for (int i = 0; i < slide_count; ++i) {
        Rectangle dest = thumbnail_cell_rect(i, columns, window_width, state.scroll_offset);
        // Skips thumbnails entirely outside the visible (scrolled) area —
        // avoids wasted draw calls on large decks.
        if (dest.y + dest.height < 0.0f || dest.y > static_cast<float>(window_height)) {
            continue;
        }
        const Texture2D &texture = state.thumbnails[static_cast<size_t>(i)].texture;
        DrawTexturePro(texture, flipped_source(texture), dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
    }

    int focused            = std::clamp(state.focused_index, 0, slide_count - 1);
    Rectangle focused_rect = thumbnail_cell_rect(focused, columns, window_width, state.scroll_offset);
    DrawRectangleLinesEx(focused_rect, kFocusBorderThickness, invert_color(background));
}

void unload_slide_overview(SlideOverviewState &state) {
    unload_thumbnail_textures(state);
}
