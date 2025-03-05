#ifndef QUANTITIES_H
#define QUANTITIES_H

#include <optional>

namespace tau {
    enum class quantity_t {
        percentage,
        pixel
    };
    
    struct Quantity { 
        quantity_t type;
        float value;

        template<typename T>
        T pixels_relative_to(T v) const {
            switch (type) {
            case quantity_t::percentage:
                return value * v;
                break;
            case quantity_t::pixel:
                return value;
                break;
            default:
                std::unreachable();
                break;
            }
        }
    };

    struct Quantity2D {
        std::optional<Quantity> x;
        std::optional<Quantity> y;
    };

    namespace literals {
        constexpr ::tau::Quantity operator ""_px (long double v) {
            return { .type = quantity_t::pixel, .value = static_cast<float>(v) };
        }

        constexpr ::tau::Quantity operator ""_px (uint64_t v) {
            return { .type = quantity_t::pixel, .value = static_cast<float>(v) };
        }

        constexpr ::tau::Quantity operator ""_per (long double v) {
            return { .type = quantity_t::percentage, .value = static_cast<float>(v) };
        }

        constexpr ::tau::Quantity operator ""_per (uint64_t v) {
            return { .type = quantity_t::percentage, .value = static_cast<float>(v) };
        }
    }
}

#ifndef TAU_NO_LITERALS
using namespace tau::literals;
#endif

#endif