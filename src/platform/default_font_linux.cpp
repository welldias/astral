#include "platform/default_font.h"

#include <fontconfig/fontconfig.h>

namespace {

// Matches `query` (fontconfig name syntax, e.g. "sans-serif" or
// "sans-serif:bold:italic") and returns the matched pattern, or nullptr if
// fontconfig isn't available. The caller is responsible for FcPatternDestroy.
FcPattern *match_pattern(const char *query) {
    if (!FcInit()) {
        return nullptr;
    }

    FcPattern *pattern = FcNameParse(reinterpret_cast<const FcChar8 *>(query));
    if (pattern == nullptr) {
        return nullptr;
    }

    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult match_result;
    FcPattern *matched = FcFontMatch(nullptr, pattern, &match_result);
    FcPatternDestroy(pattern);

    return matched;
}

std::string pattern_file_path(FcPattern *pattern) {
    if (pattern == nullptr) {
        return "";
    }
    FcChar8 *file_path = nullptr;
    if (FcPatternGetString(pattern, FC_FILE, 0, &file_path) == FcResultMatch && file_path != nullptr) {
        return reinterpret_cast<const char *>(file_path);
    }
    return "";
}

bool pattern_is_bold(FcPattern *pattern) {
    int weight = 0;
    return FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight) == FcResultMatch && weight >= FC_WEIGHT_BOLD;
}

bool pattern_is_italic(FcPattern *pattern) {
    int slant = 0;
    return FcPatternGetInteger(pattern, FC_SLANT, 0, &slant) == FcResultMatch && slant >= FC_SLANT_ITALIC;
}

// FcFontMatch always returns "some" result (with fallback), even without
// the requested variant actually being available; that's why we check
// whether the result really satisfies `wants_bold`/`wants_italic` before
// accepting it.
std::string match_variant(const char *query, bool wants_bold, bool wants_italic) {
    FcPattern *matched = match_pattern(query);
    if (matched == nullptr) {
        return "";
    }

    bool ok            = (!wants_bold || pattern_is_bold(matched)) && (!wants_italic || pattern_is_italic(matched));
    std::string result = ok ? pattern_file_path(matched) : "";

    FcPatternDestroy(matched);
    return result;
}

} // namespace

std::string get_system_default_font_path() {
    return match_variant("sans-serif", false, false);
}

std::string get_system_bold_font_path() {
    return match_variant("sans-serif:bold", true, false);
}

std::string get_system_italic_font_path() {
    return match_variant("sans-serif:italic", false, true);
}

std::string get_system_bold_italic_font_path() {
    return match_variant("sans-serif:bold:italic", true, true);
}

std::string get_system_monospace_font_path() {
    return match_variant("monospace", false, false);
}
