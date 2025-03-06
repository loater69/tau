#pragma once

#include "instance.h"

namespace tau {
    struct test {
        alignas(4) float w = 2.0f;
        alignas(4) float r = 4.0f;
        alignas(16) color c = tau::red;
    };

    template<typename Shader>
    void view<Shader>::element::render(Instance& instance, int current_frame, vk::raii::CommandBuffer& cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline.pipeline);

        float w = 0.5f;// (float)bounds.width / (float)instance.swapchain.extent.width;
        float h = 0.5f;// (float)bounds.height / (float)instance.swapchain.extent.height;

        float x = 0.5f;// ((float)bounds.left / (float)instance.swapchain.extent.width) - 0.5f * w;
        float y = 0.5f;// ((float)bounds.top / (float)instance.swapchain.extent.height) - 0.5f * h;

        style.write((char*)pipeline->buffers[current_frame].mapped);

        // test t{};
        // std::memcpy(pipeline->buffers[current_frame].mapped, &t, sizeof(test));

        BoxConstants c;
        c.position = { x, y };
        c.scale = { w, h };
        c.dimensions = { (int32_t)instance.swapchain.extent.width, (int32_t)instance.swapchain.extent.height };

        cmd.pushConstants<BoxConstants>(*pipeline->pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, { c });

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline.layout, 0, { *pipeline->sets[current_frame] }, nullptr);

        cmd.draw(6, 1, 0, 0);
    }
}