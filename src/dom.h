#ifndef DOM_H
#define DOM_H

#include "box.h"
#include "instance.h"

#include <functional>
#include <sstream>

class Instance;
struct Pipeline;

struct vec4 {
    vec4() = default;
    constexpr inline vec4(uint32_t v) {
        r = static_cast<float>(v & 0xff) / 250.0f;
        g = static_cast<float>((v >> 8) & 0xff) / 250.0f;
        b = static_cast<float>((v >> 16) & 0xff) / 250.0f;
        a = static_cast<float>((v >> 24) & 0xff) / 250.0f;
    }

    union {
        struct {
            float x;
            float y;
            float z;
            float w;
        };
        struct {
            float r;
            float g;
            float b;
            float a;
        };
    };
};

using color = vec4;

constexpr inline color white = 0xffffffff;
constexpr inline color red = 0xff0000ff;
constexpr inline color green = 0x00ff00ff;
constexpr inline color blue = 0x0000ffff;

struct Style {
    int depth = 1;

    template<std::derived_from<Style> Left, std::derived_from<Style> Right>
    struct Convolved;

    template<typename This, std::derived_from<Style> Right>
    Convolved<This, Right> operator | (this This&& self, Right&& r);

    template<typename T>
    std::string compile(this T&) {
        std::ostringstream code;
        std::ostringstream functions;

        size_t n = 0;

        T::code(n, code, functions);

        std::string c = "#version 450\nlayout(location = 0) out vec4 outColor;\nlayout(location = 0) in vec2 uv;\n";

        c += functions.str();

        c += "void main() {\n";
        c += code.str();
        c += "}\n";

        return c;
    }
};

template<std::derived_from<Style> Left, std::derived_from<Style> Right>
struct Style::Convolved : Style {
    Left l;
    Right r;

    static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions) {
        ++n;
        Left::code(n, code, functions);
        Right::code(n, code, functions);
    }
};

template<typename This, std::derived_from<Style> Right>
Style::Convolved<This, Right> Style::operator | (this This&& self, Right&& r) {
    Convolved<This, Right> c;
    c.depth = self.depth + r.depth;
    c.l = std::move(self);
    c.r = std::move(r);

    return c;
}

struct Default : Style {

    constexpr static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions) {
        ++n;
    }
};

struct Border : Style {
    int corner_radius = 0;
    int width = 0;
    color color;

    static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions) {
        functions << "float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {\n"
        << "return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;\n"
        << "}\n";

        code << "float d = roundedBoxSDF(uv - vec2(0.5), vec2(0.5), 0.2);\n"
        << "float d2 = roundedBoxSDF(uv - vec2(0.5), vec2(0.5 - (ubo.border_width" << n << " / 2.0)), 0.2);\n"
        << "if (d > 0.0) discard;\n"
        << "outColor = d2 > 0.0 ? ubo.border_color" << n << " : outColor;\n";

        ++n;
    }
};

struct Gradient : Style {
    color from;
    color to;

    static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions) {
        code << "outColor = mix(ubo.from" << n << ", ubo.to" << n << ", uv.y)\n";

        ++n;
    }
};

constexpr inline Gradient gradient{};

struct element {
    Box bounds;
    std::function<Box(Box, const std::span<Box>, size_t i)> layout;
};

#define element_props Shader style; \
std::function<Box(Box, const std::span<Box>, size_t i)> layout;

template<typename Shader = Default>
struct view {
    element_props

    struct element : ::element {

    };

    std::unique_ptr<element> operator ()() {
        auto e = std::make_unique<element>();
        // e.props = std::move(style);
        // e.layout = std::move(layout);

        return e;
    }
};

#endif