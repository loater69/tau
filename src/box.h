#ifndef BOX_H
#define BOX_H

#include <cstdint>
#include <optional>

namespace tau {
    struct Box {
        uint32_t left;
        uint32_t top;
        uint32_t width;
        uint32_t height;
    };
    
    struct alignas(8) vec2 {
        float x;
        float y;
    };
    
    struct alignas(8) ivec2 {
        int32_t x;
        int32_t y;
    };
    
    struct alignas(16) BoxConstants {
        vec2 position;
        vec2 scale;
        ivec2 dimensions;
    };
}
    
#endif
