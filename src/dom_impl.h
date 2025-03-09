#ifndef DOM_IMPL_H
#define DOM_IMPL_H

#include "instance.h"

namespace tau {
    template<typename Shader>
    void view<Shader>::element::render(Instance& instance, int current_frame, vk::raii::CommandBuffer& cmd) {
        for (size_t i = 0; i < children.size(); ++i) children[i]->render(instance, current_frame, cmd);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline.pipeline);
        
        float w = (float)content.width / (float)instance.swapchain.extent.width;
        float h = (float)content.height / (float)instance.swapchain.extent.height;
        
        float x = -1.0f + (2.0f * (float)content.left / (float)instance.swapchain.extent.width) + w;
        float y = -1.0f + (2.0f * (float)content.top / (float)instance.swapchain.extent.height) + h;
        
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

#endif