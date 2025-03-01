#ifndef INSTANCE_H
#define INSTANCE_H

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <string>

#include "box.h"
#include "dom.h"

#include <vector>

class Instance;

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

struct Pipeline {
    vk::raii::PipelineLayout layout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;
};

class Instance {
public:
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
    vk::raii::DescriptorPool descriptorPool = nullptr;
    Pipeline pipe;

    int currentFrame = 0;
    bool framebufferResized = false;

    Instance();
    void loop();
    void frame();
    void recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t image);

    vk::raii::SurfaceKHR createSurface();

    vk::raii::PhysicalDevice createPhysicalDevice();
    vk::raii::Device createDevice();
    Swapchain createSwapchain();
    void createFramebuffersForSwapchain(Swapchain &swapchain);
    Image createDepthTexture();

    vk::raii::CommandPool createCommandPool();
    vk::raii::CommandBuffers createCommandBuffers();
    vk::raii::RenderPass createRenderPass();
    Pipeline createPipeline(const std::string& vert, const std::string& frag);

    Image createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlagBits usage, vk::MemoryPropertyFlagBits properties, vk::ImageAspectFlagBits aspect);
    vk::raii::ImageView createImageView(VkImage image, vk::Format format, vk::ImageAspectFlagBits aspectFlags);
    vk::raii::CommandBuffer beginSingleCommand();
    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    // void draw(element<Gradient>& e, vk::raii::CommandBuffer& cmd);
    
    ~Instance();
};

#endif