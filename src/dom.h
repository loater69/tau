#ifndef DOM_H
#define DOM_H

#include <functional>
#include <sstream>
#include <optional>
#include <typeindex>
#include <sstream>
#include <memory>
#include <vulkan/vulkan_raii.hpp>
#include <iostream>

#include "box.h"
#include "quantities.h"

namespace tau {
    class Instance;
    struct Pipeline;

    struct vec4 {
        vec4() = default;
        constexpr inline vec4(uint32_t v) {
            a = static_cast<float>(v & 0xff) / 255.0f;
            b = static_cast<float>((v >> 8) & 0xff) / 255.0f;
            g = static_cast<float>((v >> 16) & 0xff) / 255.0f;
            r = static_cast<float>((v >> 24) & 0xff) / 255.0f;
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

    constexpr color rgb(int r, int g, int b) {
        return 0xff | (r << 24) | (g << 16) | (b << 8);
    }

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
        std::string compile(this const T&) {
            std::ostringstream code;
            std::ostringstream functions;
            std::ostringstream ubo;

            size_t n = 0;

            T::code(n, code, functions, ubo);

            std::string c = "#version 450\nlayout(location = 0) out vec4 outColor;\nlayout(location = 0) in vec2 uv;\nlayout(location = 1) in vec2 dim;\n";

            c += "layout(binding = 0) uniform UBO {\n";

            c += ubo.str();

            c += "} ubo;\n";

            c += functions.str();

            c += "void main() {\n";
            c += code.str();
            c += "}\n";

            return c;
        }

        template<typename T>
        size_t get_size(this const T&) {
            return T::size(0);
        }

        template<typename T>
        void write(this const T& t, char* p) {
            t.write_to(p);
        }
    };

    template<std::derived_from<Style> Left, std::derived_from<Style> Right>
    struct Style::Convolved : Style {
        Left l;
        Right r;

        static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions, std::ostringstream& ubo) {
            ++n;
            Left::code(n, code, functions, ubo);
            Right::code(n, code, functions, ubo);
        }

        static size_t size(size_t n) {
            n = Left::size(n);
            n = Right::size(n);

            return n;
        }

        void write_to(char*& p) const {
            l.write_to(p);
            r.write_to(p);
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

        constexpr static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions, std::ostringstream& ubo) {
            ++n;
        }
    };

    struct Border : Style {
        int corner_radius = 0;
        int width = 0;
        color color;

        static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions, std::ostringstream& ubo) {
            functions << "float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {\n"
            << "return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;\n"
            << "}\n";

            code << "vec2 size = dim;\nfloat d = roundedBoxSDF(uv * size - (size * 0.5), size * 0.5, ubo.border_radius" << n << ");\n"
            << "float d2 = roundedBoxSDF(uv * size - (size * 0.5), size * 0.5 - vec2(ubo.border_width" << n << "), ubo.border_radius" << n << " - ubo.border_width" << n << ");\n"
            << "if (d > 0.0) discard;\n"
            << "outColor = d2 > 0.0 ? ubo.border_color" << n << " : outColor;\n";

            ubo << "float border_width" << n << ";\n";
            ubo << "float border_radius" << n << ";\n";
            ubo << "vec4 border_color" << n << ";\n";

            ++n;
        }

        static size_t size(size_t n) {
            while ((n & 0b11) != 0) ++n;

            n += 4;

            while ((n & 0b11) != 0) ++n;
            
            n += 4;

            while ((n & 0b1111) != 0) ++n;

            n += 16;

            return n;
        }

        void write_to(char*& p) const {
            float v = width;
            
            while (((size_t)p & 0b11) != 0) ++p;

            *(float*)p = v;

            p += 4;

            v = corner_radius;

            while (((size_t)p & 0b11) != 0) ++p;
            *(float*)p = v;
            
            p += 4;

            while (((size_t)p & 0b1111) != 0) ++p;
        
            *(tau::color*)p = color;

            p += 16;
        }
    };

    struct Gradient : Style {
        color from;
        color to;

        static void code(size_t& n, std::ostringstream& code, std::ostringstream& functions, std::ostringstream& ubo) {
            code << "outColor = mix(ubo.from" << n << ", ubo.to" << n << ", uv.y);\n";

            ubo << "vec4 from" << n << ";\nvec4 to" << n << ";\n";

            ++n;
        }

        static size_t size(size_t n) {
            while ((n & 0b1111) != 0) ++n;

            n += 16;

            while ((n & 0b1111) != 0) ++n;

            n += 16;

            return n;
        }

        void write_to(char*& p) const {
            while (((size_t)p & 0b1111) != 0) ++p;

            *(color*)p = from;

            p += 16;

            while (((size_t)p & 0b1111) != 0) ++p;

            *(color*)p = to;

            p += 16;
        }
    };

    struct element;
    
    struct Layout {
        template<typename T>
        bool is() const {
            return dynamic_cast<const T*>(this) != nullptr;
        }

        template<typename T>
        T* to() {
            return dynamic_cast<T*>(this);
        }

        virtual Box layout(Box, element& el) const = 0;
    };
    
    struct element {
        Box bounds;
        Box content;
        std::unique_ptr<Layout> layout;
        std::vector<std::unique_ptr<element>> children;

        virtual void render(Instance& instance, int current_frame, vk::raii::CommandBuffer&) = 0;
    };
    
    using elements = std::vector<std::unique_ptr<element>>;

    struct Block {
        Quantity2D dimensions;
        Quantity2D padding;
        Quantity2D margin;

        struct Impl : Layout {
            Quantity2D dimensions;
            Quantity2D padding;
            Quantity2D margin;

            Box layout(Box available, element& el) const {
                Box b = available;

                if (dimensions.x.has_value()) b.width = dimensions.x.value().pixels_relative_to(available.width);

                if (dimensions.y.has_value()) b.height = dimensions.y.value().pixels_relative_to(available.width);

                Box contentAv = b;

                contentAv.left += margin.x.value_or(0_px).pixels_relative_to(available.width);
                contentAv.left += padding.x.value_or(0_px).pixels_relative_to(b.width);

                contentAv.width -= 2 * margin.x.value_or(0_px).pixels_relative_to(available.width);
                contentAv.width -= 2 * padding.x.value_or(0_px).pixels_relative_to(b.width);

                contentAv.top += margin.y.value_or(0_px).pixels_relative_to(available.width);
                contentAv.top += padding.y.value_or(0_px).pixels_relative_to(b.width);

                contentAv.height -= 2 * margin.y.value_or(0_px).pixels_relative_to(available.width);
                contentAv.height -= 2 * padding.y.value_or(0_px).pixels_relative_to(b.width);

                for (size_t i = 0; i < el.children.size(); ++i) {
                    el.children[i]->bounds = el.children[i]->layout->layout(contentAv, *el.children[i]);
                    
                    contentAv.top += el.children[i]->bounds.height;
                    // contentAv.height -= el.children[i]->bounds.height;
                }

                if (!dimensions.y.has_value()) b.height = contentAv.top + margin.y.value_or(0_px).pixels_relative_to(available.width) + padding.y.value_or(0_px).pixels_relative_to(b.width);

                el.content = b;

                el.content.left += margin.x.value_or(0_px).pixels_relative_to(available.width);
                el.content.width -= 2 * margin.x.value_or(0_px).pixels_relative_to(available.width);

                el.content.top += margin.y.value_or(0_px).pixels_relative_to(available.width);
                el.content.height -= 2 * margin.y.value_or(0_px).pixels_relative_to(available.width);

                return b;
            }
        };

        operator std::unique_ptr<Layout>() const {
            auto impl = std::make_unique<Impl>();

            impl->dimensions = dimensions;
            impl->padding = padding;
            impl->margin = margin;

            return impl;
        }
    };

    struct Horizontal {

    };

    struct Flex {
        Quantity2D dimensions;
        Quantity2D padding;
        Quantity2D margin;

        struct Impl : Layout {
            Quantity2D dimensions;
            Quantity2D padding;
            Quantity2D margin;

            Box preferredLayout(Box b, const std::span<std::unique_ptr<element>> els) const {
                // left-to-right

                // css-flexbox-1 9.2: available main and cross space
                Box available;

                if (dimensions.x.has_value()) available.width = dimensions.x.value().pixels_relative_to(b.width);
                else {

                }

                if (dimensions.y.has_value()) available.height = dimensions.y.value().pixels_relative_to(b.width);
                else available.height = std::numeric_limits<uint32_t>::max();

                available.width -= 2 * padding.x.value_or(0_px).pixels_relative_to(b.width);
                available.width -= 2 * margin.x.value_or(0_px).pixels_relative_to(b.width);

                available.height -= 2 * padding.y.value_or(0_px).pixels_relative_to(b.width);
                available.height -= 2 * margin.y.value_or(0_px).pixels_relative_to(b.width);

                uint32_t max_h = 0;
                uint32_t width = 0;

                for (size_t i = 0; i < els.size(); ++i) {
                    /* auto box = els[i]->layout->layout({
                        .width = b.width - 2 * padding.x.value_or(0_px).pixels_relative_to(width) - 2 * margin.x.value_or(0_px).pixels_relative_to(b.width) - width,
                        .height = b.height - 2 * padding.y.value_or(0_px).pixels_relative_to(width) - 2 * margin.y.value_or(0_px).pixels_relative_to(b.height)
                    }, );

                    width += box.width;

                    if (box.height > max_h) max_h = box.height; */
                }

                if (dimensions.x.has_value()) width = dimensions.x.value().value;
                if (dimensions.y.has_value()) max_h = dimensions.y.value().value;

                width += 2 * padding.x.value_or(0_px).pixels_relative_to(width);
                max_h += 2 * padding.y.value_or(0_px).pixels_relative_to(width);

                width += 2 * margin.x.value_or(0_px).pixels_relative_to(b.width);
                max_h += 2 * margin.y.value_or(0_px).pixels_relative_to(b.height);

                return {
                    .width = width,
                    .height = max_h
                };
            }
    
            Box layout(Box, element&) const {
                return Box{ .left = 10, .top = 10, .width = 200, .height = 50 };
            }
        };

        operator std::unique_ptr<Layout>() const {
            auto impl = std::make_unique<Impl>();

            impl->dimensions = dimensions;
            impl->padding = padding;
            impl->margin = margin;

            return impl;
        }
    };

    struct PipelineCacheEntry;

    #define element_props \
    std::unique_ptr<Layout> layout; \
    Shader style; \

    template<typename Shader = Default>
    struct view {
        element_props

        struct element : ::tau::element {
            PipelineCacheEntry* pipeline;
            Shader style;

            void render(Instance& instance, int current_frame, vk::raii::CommandBuffer&);
        };

        std::unique_ptr<element> operator ()(elements&& els = elements{}) {
            auto e = std::make_unique<element>();

            e->pipeline = Instance::current_instance->template get_shader<Shader>();
            e->layout = std::move(layout);
            e->style = std::move(style);
            e->children = std::move(els);

            return e;
        }
    };

    struct Component {
        struct element : tau::element {
            std::function<std::unique_ptr<tau::element>()> render_func;
            std::unique_ptr<tau::element> child;
            std::type_index type = typeid(void);

            void render(Instance& instance, int current_frame, vk::raii::CommandBuffer&);

            /* inline void diff_check(std::stack<diff_entry>& s) noexcept {
                auto t = s.top();
                s.pop();

                // std::cout << "component\n";

                // std::cout << std::boolalpha << t.old->get()->is<comp_t>() << address_of(t.replacement->get()->to<comp_t>()->render) << "cey\n";

                // std::cout << t.replacement->get()->to<comp_t>()->type.name() << ", " << t.old->get()->to<comp_t>()->type.name() << '\n';

                // auto x = t.replacement->get()->to<comp_t>(); //->render;

                if (!t.replacement->get()->is<comp_t>() || (t.replacement->get()->to<comp_t>()->render.target_type() != t.old->get()->to<comp_t>()->render.target_type())) {
                    // std::cout << "adslkj\n";
                    html::set_content(*view, **t.old, t.replacement->get()->first_render());
                    *t.old = std::move(*t.replacement);
                }

                // std::cout << "end\n";
            } */
        };

        inline static thread_local element* current_component = nullptr;
    };

    using ComponentElement = Component::element;

    template<typename T = std::monostate>
    const auto component(auto r) {
        static auto render = std::move(r);

        struct DerivedComponent : Component {
            inline std::unique_ptr<element> operator ()(T&& t) const {
                std::unique_ptr<element> c = std::make_unique<element>();

                // std::cerr << "hey\n";

                // const_cast because why not
                // c->c = const_cast<comp_t*>(static_cast<const comp_t*>(this));

                Component::current_component = c.get();

                if constexpr (!std::is_same_v<T, std::monostate>) {
                    c->render_func = [arg = std::move(t), ptr = c.get(), r = render]() mutable -> std::unique_ptr<tau::element> { Component::current_component = ptr; return r(arg); }; // std::bind(render, std::move(t));
                } else c->render_func = [ptr = c.get(), r = render]() mutable -> std::unique_ptr<tau::element> { Component::current_component = ptr; return r(); };// render;
                c->type = typeid(render);
                /* c->render = [; */

                // c->render.

                return std::move(c);
            }
        };

        return DerivedComponent{};
    }
    /* template<typename Shader>
    void view<Shader>::element::render(Instance& instance, int current_frame, vk::raii::CommandBuffer& cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline.pipeline);

        float w = (float)bounds.width / (float)instance.swapchain.extent.width;
        float h = (float)bounds.height / (float)instance.swapchain.extent.height;

        float x = ((float)bounds.left / (float)instance.swapchain.extent.width) + 0.5f * w;
        float y = ((float)bounds.top / (float)instance.swapchain.extent.height) + 0.5f * h;

        BoxConstants c;
        c.position = { x, y };
        c.scale = { w, h };
        c.dimensions = { (int32_t)instance.swapchain.extent.width, (int32_t)instance.swapchain.extent.height };

        cmd.pushConstants<BoxConstants>(*pipeline->pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, { c });

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline.layout, 0, { pipeline->sets[current_frame] }, nullptr);

        cmd.draw(6, 1, 0, 0);
    } */

    elements operator | (std::unique_ptr<element>&&, std::unique_ptr<element>&&);
    elements operator | (elements&&, std::unique_ptr<element>&&);
}

#endif