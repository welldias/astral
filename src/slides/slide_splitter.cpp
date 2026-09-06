#include "slides/slide_splitter.h"

#include <cctype>
#include <optional>
#include <sstream>

namespace {

bool hex_digit_value(char c, int &out) {
    if (c >= '0' && c <= '9') {
        out = c - '0';
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out = 10 + (c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out = 10 + (c - 'A');
        return true;
    }
    return false;
}

// Converts a pair of hex digits (`hi`, `lo`) into a byte 0-255; false if
// either one isn't a valid hexadecimal digit.
bool hex_byte(char hi, char lo, unsigned char &out) {
    int a = 0;
    int b = 0;
    if (!hex_digit_value(hi, a) || !hex_digit_value(lo, b)) {
        return false;
    }
    out = static_cast<unsigned char>(a * 16 + b);
    return true;
}

// Short form "#rgb"/"#rgba": each digit counts for both nibbles of the byte
// (e.g. 'f' -> 0xff).
bool hex_digit_doubled(char c, unsigned char &out) {
    return hex_byte(c, c, out);
}

// Parses a color in the web format: "#rgb", "#rgba", "#rrggbb" or
// "#rrggbbaa" (case-insensitive). Without the leading '#', with a length
// that's none of these, or with a non-hexadecimal digit, returns nullopt —
// the parameter is treated as invalid (see apply_param) and ignored.
std::optional<Color> parse_web_color(const std::string &value) {
    if (value.empty() || value[0] != '#') {
        return std::nullopt;
    }
    std::string hex = value.substr(1);

    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;

    switch (hex.size()) {
    case 3:
        if (!hex_digit_doubled(hex[0], r) || !hex_digit_doubled(hex[1], g) || !hex_digit_doubled(hex[2], b)) {
            return std::nullopt;
        }
        break;
    case 4:
        if (!hex_digit_doubled(hex[0], r) || !hex_digit_doubled(hex[1], g) || !hex_digit_doubled(hex[2], b) || !hex_digit_doubled(hex[3], a)) {
            return std::nullopt;
        }
        break;
    case 6:
        if (!hex_byte(hex[0], hex[1], r) || !hex_byte(hex[2], hex[3], g) || !hex_byte(hex[4], hex[5], b)) {
            return std::nullopt;
        }
        break;
    case 8:
        if (!hex_byte(hex[0], hex[1], r) || !hex_byte(hex[2], hex[3], g) || !hex_byte(hex[4], hex[5], b) || !hex_byte(hex[6], hex[7], a)) {
            return std::nullopt;
        }
        break;
    default:
        return std::nullopt;
    }

    return Color{ r, g, b, a };
}

std::string trim(const std::string &s) {
    size_t start = 0;
    size_t end   = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

// Applies a "key=value" parameter, already trimmed of surrounding
// whitespace, to `params`. An unknown key or unrecognized value is
// silently ignored — a marker with a typo shouldn't break the slide split,
// it just fails to change that parameter's default.
void apply_param(const std::string &key, const std::string &value, SlideParams &params) {
    if (key == "align") {
        if (value == "left") {
            params.align = TextAlign::Left;
        } else if (value == "right") {
            params.align = TextAlign::Right;
        } else if (value == "center") {
            params.align = TextAlign::Center;
        }
    } else if (key == "bg-color") {
        if (std::optional<Color> color = parse_web_color(value)) {
            params.bg_color = color;
        }
    } else if (key == "text-color") {
        if (std::optional<Color> color = parse_web_color(value)) {
            params.text_color = color;
        }
    } else if (key == "block-color") {
        if (std::optional<Color> color = parse_web_color(value)) {
            params.block_color = color;
        }
    } else if (key == "transition") {
        if (std::optional<TransitionKind> transition = parse_transition_kind(value)) {
            params.transition = transition;
        }
    }
}

// Reads the "key=value, key=value, ..." list from inside the marker's
// brackets and applies each recognized pair to `params`.
void parse_params(const std::string &content, SlideParams &params) {
    size_t start = 0;
    while (start <= content.size()) {
        size_t comma      = content.find(',', start);
        std::string token = trim(content.substr(start, comma == std::string::npos ? std::string::npos : comma - start));

        if (!token.empty()) {
            size_t eq = token.find('=');
            if (eq != std::string::npos) {
                apply_param(trim(token.substr(0, eq)), trim(token.substr(eq + 1)), params);
            }
            // Token without '=' (a name-only parameter, no value) is ignored — no
            // parameter recognized today has that form.
        }

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
}

// If `line` is a slide marker line ("$[...]", possibly preceded by
// whitespace), extracts the parameters from inside the brackets into
// `out_params` (starting from SlideParams' default values) and returns
// true. Otherwise returns false and leaves `out_params` untouched.
bool try_parse_marker(const std::string &line, SlideParams &out_params) {
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    if (line.compare(i, 2, "$[") != 0) {
        return false;
    }

    size_t content_start = i + 2;
    size_t close         = line.find(']', content_start);
    if (close == std::string::npos) {
        return false;
    }

    out_params = SlideParams{};
    parse_params(line.substr(content_start, close - content_start), out_params);
    return true;
}

} // namespace

std::vector<SlideSource> split_into_slides(const std::string &source) {
    std::vector<std::string> lines;
    {
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
    }

    std::vector<SlideSource> slides;
    std::string current_text;
    bool current_has_lines = false;
    // Parameters of the slide being accumulated — come from the marker that
    // preceded it, or SlideParams' default for the first slide (no marker
    // before it).
    SlideParams current_params{};

    auto flush_slide = [&]() {
        slides.push_back(SlideSource{ current_text, current_params });
        current_text.clear();
        current_has_lines = false;
    };

    for (const std::string &line : lines) {
        SlideParams next_params;
        if (try_parse_marker(line, next_params)) {
            // Marker before any content (e.g. the first line of the file, or
            // several markers in a row at the start): there's no real previous
            // slide to close, so it just updates the parameters of the slide
            // that's still about to start, without producing an empty slide
            // ahead of it.
            if (!slides.empty() || current_has_lines) {
                flush_slide();
            }
            current_params = next_params; // apply to the slide starting now, not the one that just closed
            continue;
        }
        if (current_has_lines) {
            current_text += '\n';
        }
        current_text += line;
        current_has_lines = true;
    }
    flush_slide(); // last slide, even if empty (or the only one, if there's no marker)

    return slides;
}
