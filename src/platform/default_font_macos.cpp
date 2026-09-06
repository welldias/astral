// NOTE: this file is only compiled in macOS builds. It could not be
// compiled/tested in the Linux development environment used for this
// project — validate it on a real macOS machine before relying on it.

#include "platform/default_font.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

namespace {

std::string font_file_path(CTFontRef font) {
    if (font == nullptr) {
        return "";
    }

    std::string result;
    CFURLRef url = static_cast<CFURLRef>(CTFontCopyAttribute(font, kCTFontURLAttribute));
    if (url != nullptr) {
        char path_buffer[1024];
        if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8 *>(path_buffer), sizeof(path_buffer))) {
            result = path_buffer;
        }
        CFRelease(url);
    }
    return result;
}

// Requests the default UI font variant that satisfies `traits` (e.g.
// kCTFontBoldTrait | kCTFontItalicTrait). CoreText returns nullptr when
// there's no face that exactly satisfies the requested traits; in that
// case we return "" (the caller must not synthesize the effect).
std::string match_variant(CTFontSymbolicTraits traits) {
    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 0.0, nullptr);
    if (font == nullptr) {
        return "";
    }

    if (traits == 0) {
        std::string result = font_file_path(font);
        CFRelease(font);
        return result;
    }

    CTFontRef variant_font = CTFontCreateCopyWithSymbolicTraits(font, 0.0, nullptr, traits, traits);
    CFRelease(font);
    if (variant_font == nullptr) {
        return "";
    }

    CTFontSymbolicTraits actual_traits = CTFontGetSymbolicTraits(variant_font);
    std::string result                 = ((actual_traits & traits) == traits) ? font_file_path(variant_font) : "";

    CFRelease(variant_font);
    return result;
}

} // namespace

std::string get_system_default_font_path() {
    return match_variant(0);
}

std::string get_system_bold_font_path() {
    return match_variant(kCTFontBoldTrait);
}

std::string get_system_italic_font_path() {
    return match_variant(kCTFontItalicTrait);
}

std::string get_system_bold_italic_font_path() {
    return match_variant(kCTFontBoldTrait | kCTFontItalicTrait);
}

std::string get_system_monospace_font_path() {
    // kCTFontUIFontUserFixedPitch is the system's default monospace font
    // (the monospace equivalent of kCTFontUIFontSystem).
    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, 0.0, nullptr);
    if (font == nullptr) {
        return "";
    }

    std::string result = font_file_path(font);
    CFRelease(font);
    return result;
}
