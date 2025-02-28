#ifndef DOM_H
#define DOM_H

#include "box.h"
#include "instance.h"

#include <functional>

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

struct Default {
    struct Fragstate {};
};

struct Gradient {
    struct Fragstate {
        color from;
        color to;
    };

    struct Staticstate {
        vk::raii::DescriptorSetLayout layout = nullptr;
        Pipeline pipe;
    };

    static constexpr auto vert = "vert.spv";
    static constexpr auto frag = "gradient.spv";
    inline static Staticstate state;
    
    static vk::raii::DescriptorSetLayout createDescriptorSetLayout(Instance& i);
    static vk::raii::DescriptorSet createDescriptorSet(Instance& i);
};

constexpr inline Gradient gradient{};

struct element {
    Box bounds;
    std::function<Box(Box, const std::span<Box>, size_t i)> layout;

    virtual void render(vk::raii::CommandBuffer& cmd) = 0;
};

#define element_props [[no_unique_address]] [[msvc::no_unique_address]] Shader type; \
Shader::Fragstate style; \
std::function<Box(Box, const std::span<Box>, size_t i)> layout;

template<typename Shader = Default>
struct view { 
    element_props

    struct element : ::element {
        Shader::Fragstate props;

        virtual void render(vk::raii::CommandBuffer& cmd) {
            BoxConstants bc;

            cmd.pushConstants(Shader::state.layout, vk::ShaderStageFlagBits::eVertex, 0, { bc });
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics);
            cmd.drawIndexed(6, 1, 0, 0, 0);
        }
    };

    std::unique_ptr<element> operator ()() {
        auto e = std::make_unique<element>();
        e.props = std::move(style);
        e.layout = std::move(layout);

        return e;
    }
};

#endif