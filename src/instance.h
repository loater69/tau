#ifndef INSTANCE_H
#define INSTANCE_H

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <fstream>

#include "box.h"
#include "dom.h"

#include <vector>
#include <map>
#include <typeindex>
#include <memory>
#include <sstream>

#include <stb_truetype.h>

namespace tau {
    class Instance;
    struct element;
    
    struct Swapchain {
        vk::raii::SwapchainKHR swapchain{nullptr};
        std::vector<VkImage> images;
        std::vector<vk::raii::ImageView> imageViews;
        std::vector<vk::raii::Framebuffer> framebuffers;
        vk::Format format;
        vk::Extent2D extent;
        
        void recreate(Instance& instance);
    };
    
    struct Image {
        vk::raii::Image image = nullptr;
        vk::raii::ImageView view = nullptr;
        vk::raii::DeviceMemory memory = nullptr;
    };

    struct CombinedImage {
        Image img;
        vk::raii::Sampler sampler = nullptr;
    };
    
    struct Pipeline {
        vk::raii::PipelineLayout layout = nullptr;
        vk::raii::Pipeline pipeline = nullptr;
    };

    struct UniformBuffer {
        vk::raii::Buffer buffer = nullptr;
        vk::raii::DeviceMemory memory = nullptr;
        void* mapped = nullptr;
    };

    struct PipelineCacheEntry {
        Pipeline pipeline;
        vk::raii::DescriptorPool descriptorPool = nullptr;
        vk::raii::DescriptorSetLayout layout = nullptr;
        std::vector<vk::raii::DescriptorSet> sets;
        std::vector<UniformBuffer> buffers;
    };

    struct FontChar {
        uint8_t* data;
        uint8_t w;
        uint8_t h;
        int8_t x;
        int8_t y;
        float advance;
    };

    struct Font {
        stbtt_fontinfo info;
        std::vector<char> buffer;
        float scale;
        std::array<FontChar, 256> chars;

        ~Font();
    };
    
    class Instance {
    public:
        inline static thread_local Instance* current_instance = nullptr;
        static constexpr auto max_frames_in_flight = 2;
        
        vk::raii::Context ctx;
        vk::raii::Instance instance = nullptr;
        GLFWwindow* window;
        vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
        vk::raii::PhysicalDevice physicalDevice = nullptr;
        vk::raii::Device device = nullptr;
        vk::raii::Queue graphicsQueue = nullptr;
        vk::raii::Queue presentQueue = nullptr;
        vk::raii::SurfaceKHR surface = nullptr;
        Swapchain swapchain;
        vk::raii::CommandPool commandPool = nullptr;
        vk::raii::CommandBuffers commandBuffers = nullptr;
        vk::raii::RenderPass renderPass = nullptr;
        Image depthTexture;
        std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
        std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
        std::vector<vk::raii::Fence> inFlightFences;

        std::map<std::type_index, PipelineCacheEntry> pipeline_cache;
        std::map<std::string, CombinedImage> image_cache;
        std::map<std::string, Font> font_cache;

        template<typename Shader>
        PipelineCacheEntry* get_shader() {
            if (pipeline_cache.contains(typeid(Shader))) return &pipeline_cache.at(typeid(Shader));

            auto src = Shader{}.compile();

            std::ostringstream str;
            str << typeid(Shader).hash_code();

            std::ofstream(str.str() + ".frag") << src;

            std::ostringstream cmd;
            cmd << "glslc " << str.str() << ".frag -o " << str.str() << ".spv";

            system(cmd.str().c_str());

            PipelineCacheEntry entry;

            vk::DescriptorPoolSize poolSize{};
            poolSize.type = vk::DescriptorType::eUniformBuffer;
            poolSize.descriptorCount = static_cast<uint32_t>(max_frames_in_flight);

            std::vector<vk::DescriptorPoolSize> pss;
            pss.push_back(poolSize);

            Shader::poolSizes(pss);

            vk::DescriptorPoolCreateInfo dpci{};
            dpci.maxSets = 32;
            dpci.poolSizeCount = pss.size();
            dpci.pPoolSizes = pss.data();

            entry.descriptorPool = device.createDescriptorPool(dpci);

            // create descriptor set layout

            vk::DescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            binding.descriptorCount = 1;
            binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.push_back(binding);

            Shader::bindings(bindings);

            vk::DescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.bindingCount = bindings.size();
            layoutInfo.pBindings = bindings.data();

            entry.layout = device.createDescriptorSetLayout(layoutInfo);

            std::array<vk::DescriptorSetLayout, 1> sets = { *entry.layout };

            entry.pipeline = createPipeline("vert.spv", str.str() + ".spv", sets);

            std::array<vk::DescriptorSetLayout, max_frames_in_flight> arr;

            for (size_t i = 0; i < max_frames_in_flight; ++i) arr[i] = *entry.layout;

            vk::DescriptorSetAllocateInfo allocInfo{};
            allocInfo.descriptorPool = *entry.descriptorPool;
            allocInfo.descriptorSetCount = arr.size();
            allocInfo.pSetLayouts = arr.data();

            entry.sets = device.allocateDescriptorSets(allocInfo);

            auto size = Shader{}.get_size();

            if (size == 0) size = 4;

            std::cout << size << '\n';

            for (size_t i = 0; i < max_frames_in_flight; ++i) {
                UniformBuffer ub;

                auto[buf, mem] = createBuffer(size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                ub.buffer = std::move(buf);
                ub.memory = std::move(mem);

                ub.mapped = ub.memory.mapMemory(0, size);

                entry.buffers.push_back(std::move(ub));
            }
            
            for (size_t i = 0; i < max_frames_in_flight; ++i) {
                vk::DescriptorBufferInfo bi{};
                bi.buffer = *entry.buffers[i].buffer;
                bi.offset = 0;
                bi.range = size;
                
                vk::WriteDescriptorSet descriptorWrite{};
                descriptorWrite.dstSet = *entry.sets[i];
                descriptorWrite.dstBinding = 0;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bi;

                device.updateDescriptorSets({ descriptorWrite }, nullptr);
            }

            pipeline_cache[typeid(Shader)] = std::move(entry);

            return &pipeline_cache[typeid(Shader)];
        }

        CombinedImage* getImage(std::string& img);
        Font* getFont(std::string& font);

        std::unique_ptr<ComponentElement> top_component;
        
        int currentFrame = 0;
        bool framebufferResized = false;
        
        Instance();
        void loop();

        void render(std::unique_ptr<ComponentElement>&& comp);

        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

        void frame();
        void recordCommandBuffer(vk::raii::CommandBuffer& cmd, int frame, uint32_t image);
        
        vk::raii::SurfaceKHR createSurface();
        
        vk::raii::PhysicalDevice createPhysicalDevice();
        vk::raii::Device createDevice();
        Swapchain createSwapchain();
        void createFramebuffersForSwapchain(Swapchain &swapchain);
        Image createDepthTexture();
        
        vk::raii::CommandPool createCommandPool();
        vk::raii::CommandBuffers createCommandBuffers();
        vk::raii::RenderPass createRenderPass();
        Pipeline createPipeline(const std::string& vert, const std::string& frag, std::span<vk::DescriptorSetLayout> sets = std::span<vk::DescriptorSetLayout>{});
        
        Image createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlagBits properties, vk::ImageAspectFlagBits aspect);
        Image loadColorTexture(const char* path);
        vk::raii::ImageView createImageView(VkImage image, vk::Format format, vk::ImageAspectFlagBits aspectFlags);
        vk::raii::CommandBuffer beginSingleCommand();
        void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
        // void draw(element<Gradient>& e, vk::raii::CommandBuffer& cmd);
        
        ~Instance();
    };
}

#include "dom_impl.h"

#endif