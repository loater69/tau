#include "instance.h"

#include <map>
#include <set>
#include <string>
#include <optional>
#include <span>
#include <limits>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}

void tau::Instance::loop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        frame();
    }
}

void tau::Instance::render(std::unique_ptr<ComponentElement>&& comp) {
    comp->child = comp->render_func();
    comp->child->bounds = comp->child->layout->layout({
        .left = 0,
        .top = 0,
        .width = swapchain.extent.width,
        .height = swapchain.extent.height
    }, *comp->child);

    top_component = std::move(comp);

    loop();
}

static void frameBufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<tau::Instance*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

uint32_t findMemoryType(const vk::raii::PhysicalDevice& pd, uint32_t typeFilter, vk::MemoryPropertyFlagBits properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = pd.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> tau::Instance::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    auto buffer = device.createBuffer(bufferInfo);

    auto memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, *(vk::MemoryPropertyFlagBits*)&properties); // fuck it

    auto memory = device.allocateMemory(allocInfo);

    buffer.bindMemory(*memory, 0);

    return std::make_pair(std::move(buffer), std::move(memory));
}

void tau::Instance::frame() {
    device.waitForFences({ *inFlightFences[currentFrame] }, true, std::numeric_limits<uint64_t>::max());

    auto[res, i] = swapchain.swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), *imageAvailableSemaphores[currentFrame]);

    if (res == vk::Result::eErrorOutOfDateKHR) {
        swapchain.recreate(*this);
        top_component->child->bounds = top_component->child->layout->layout({
            .left = 0,
            .top = 0,
            .width = swapchain.extent.width,
            .height = swapchain.extent.height
        }, *top_component->child);
        return;
    } else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    device.resetFences({ *inFlightFences[currentFrame] });

    commandBuffers[currentFrame].reset();
    recordCommandBuffer(commandBuffers[currentFrame], currentFrame, i);

    vk::SubmitInfo submitInfo{};

    vk::Semaphore waitSemaphores[] = { *imageAvailableSemaphores[currentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffers[currentFrame];

    vk::Semaphore signalSemaphores[] = { *renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // submit that fucker
    graphicsQueue.submit({ submitInfo }, *inFlightFences[currentFrame]);

    vk::PresentInfoKHR pi{};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSemaphores;

    vk::SwapchainKHR swapChains[] = { *swapchain.swapchain };
    pi.swapchainCount = 1;
    pi.pSwapchains = swapChains;
    pi.pImageIndices = &i;

    res = presentQueue.presentKHR(pi);

    if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR || framebufferResized) {
        framebufferResized = false;
        swapchain.recreate(*this);
        top_component->child->bounds = top_component->child->layout->layout({
            .left = 0,
            .top = 0,
            .width = swapchain.extent.width,
            .height = swapchain.extent.height
        }, *top_component->child);
    } else if (res != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    ++currentFrame %= max_frames_in_flight;
}

void tau::Instance::recordCommandBuffer(vk::raii::CommandBuffer& cmd, int frame, uint32_t image) {
    cmd.begin({});

    vk::RenderPassBeginInfo rpbi{};

    rpbi.renderPass = *renderPass;
    rpbi.framebuffer = *swapchain.framebuffers[image];

    rpbi.renderArea.offset = vk::Offset2D{0, 0};
    rpbi.renderArea.extent = swapchain.extent;

    std::array values = { 0.0f, 0.0f, 0.0f, 1.0f };

    vk::ClearColorValue clearColor(values);
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpbi.pClearValues = clearValues.data();

    cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.extent.width);
    viewport.height = static_cast<float>(swapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, { viewport });

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;

    cmd.setScissor(0, { scissor });

    // cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipe.pipeline);

    // BoxConstants c;
    // c.position = { 0.5f, 0.5f };
    // c.scale = { 0.5f, 0.5f };
    // c.dimensions = { (int32_t)swapchain.extent.width, (int32_t)swapchain.extent.height };

    // cmd.pushConstants<BoxConstants>(*pipe.layout, vk::ShaderStageFlagBits::eVertex, 0, { c });

    // cmd.draw(6, 1, 0, 0);

    top_component->render(*this, frame, cmd);

    cmd.endRenderPass();

    cmd.end();
}

vk::raii::SurfaceKHR tau::Instance::createSurface() {
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(*instance, window, nullptr, &surface);

    return vk::raii::SurfaceKHR(instance, vk::SurfaceKHR(surface));
}

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice& device, vk::raii::SurfaceKHR& surface) {
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(*surface);

    details.formats = device.getSurfaceFormatsKHR(*surface);

    details.presentModes = device.getSurfacePresentModesKHR(*surface);

    return details;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool complete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

QueueFamilyIndices queueFamilies(vk::raii::PhysicalDevice& device, vk::raii::SurfaceKHR& surface) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = device.getSurfaceSupportKHR(i, *surface);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.complete()) break;

        i++;
    }

    return indices;
}

bool deviceExtensionsSupport(vk::raii::PhysicalDevice& pd) {
    auto availableExtensions = pd.enumerateDeviceExtensionProperties(nullptr);

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

int deviceScore(vk::raii::PhysicalDevice& pd, vk::raii::SurfaceKHR& surface) {
    auto features = pd.getFeatures();
    auto properties = pd.getProperties();

    int score = 0;

    score += properties.limits.maxImageDimension2D;

    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;

    if (!features.samplerAnisotropy) return 0;

    if (deviceExtensionsSupport(pd)) {
        auto swapchainDetails = querySwapChainSupport(pd, surface);

        if (swapchainDetails.formats.empty() || swapchainDetails.presentModes.empty()) return 0;
    } else return 0;

    if (!queueFamilies(pd, surface).graphicsFamily.has_value()) return 0;

    return score;
}

vk::raii::PhysicalDevice tau::Instance::createPhysicalDevice() {
    auto devices = instance.enumeratePhysicalDevices();

    std::multimap<int, vk::raii::PhysicalDevice*> scores;

    for (int i = 0; i < devices.size(); ++i) {
        scores.insert({ deviceScore(devices[i], surface), &devices[i] });
    }

    return std::move(*scores.rbegin()->second);
}

vk::raii::Device tau::Instance::createDevice() {
    QueueFamilyIndices indices = queueFamilies(physicalDevice, surface);

    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = true;

    vk::DeviceCreateInfo createInfo{};
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (true) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    return vk::raii::Device(physicalDevice, createInfo);
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    // if (enable_vsync) return VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };


        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        std::cout << "W: " << width << '\n';
        std::cout << "W: " << actualExtent.width << '\n';

        return actualExtent;
    }
}

tau::Swapchain tau::Instance::createSwapchain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = *surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = queueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    Swapchain swapchain;

    auto sw = vk::raii::SwapchainKHR(device, createInfo);

    swapchain.images = sw.getImages();
    swapchain.swapchain = std::move(sw);
    swapchain.format = surfaceFormat.format;
    swapchain.extent = extent;

    for (int i = 0; i < swapchain.images.size(); ++i) {
        swapchain.imageViews.push_back(createImageView(swapchain.images[i], surfaceFormat.format, vk::ImageAspectFlagBits::eColor));
    }

    return swapchain;
}

vk::raii::CommandPool tau::Instance::createCommandPool() {
    vk::CommandPoolCreateInfo cpci{};
    cpci.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpci.queueFamilyIndex = queueFamilies(physicalDevice, surface).graphicsFamily.value();

    return vk::raii::CommandPool(device, cpci);
}

vk::raii::CommandBuffers tau::Instance::createCommandBuffers() {
    vk::CommandBufferAllocateInfo cbai{};
    cbai.commandBufferCount = max_frames_in_flight;
    cbai.level = vk::CommandBufferLevel::ePrimary;
    cbai.commandPool = *commandPool;

    return vk::raii::CommandBuffers(device, cbai);
}

vk::Format findSupportedFormat(vk::raii::PhysicalDevice& pd, const std::span<vk::Format> candidates, vk::ImageTiling tiling, vk::FormatFeatureFlagBits features) {
    for (vk::Format format : candidates) {
        vk::FormatProperties props = pd.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    std::cerr << "can't find supported format\n";

    std::unreachable();
}

vk::Format findDepthFormat(vk::raii::PhysicalDevice& pd) {
    vk::Format candidates[] = { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };

    return findSupportedFormat(
        pd,
        candidates,
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

vk::raii::RenderPass tau::Instance::createRenderPass() {
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.format;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(physicalDevice);
    depthAttachment.samples = vk::SampleCountFlagBits::e1;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;

    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    vk::AttachmentDescription attachments[] = { colorAttachment, depthAttachment };

    vk::RenderPassCreateInfo createInfo{};
    createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    return vk::raii::RenderPass(device, createInfo);
}

vk::raii::ImageView tau::Instance::createImageView(VkImage image, vk::Format format, vk::ImageAspectFlagBits aspectFlags) {
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vk::raii::ImageView(device, viewInfo);
}

vk::raii::CommandBuffer tau::Instance::beginSingleCommand() {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = *commandPool;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo)[0]);

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

bool hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void tau::Instance::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    auto cmd = beginSingleCommand();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vk::PipelineStageFlagBits sourceStage;
    vk::PipelineStageFlagBits destinationStage;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    } else {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    cmd.pipelineBarrier(sourceStage, destinationStage, (vk::DependencyFlagBits)0, nullptr, nullptr, { barrier });

    cmd.end();

    vk::CommandBuffer cb = *cmd;

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    
    graphicsQueue.submit({ submitInfo });
    graphicsQueue.waitIdle();
}

/* void tau::Instance::draw(element<Gradient> &e, vk::raii::CommandBuffer& cmd) {
    // cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *Gradient::state.playout, 0, {});
    // cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *Gradient::state.pipeline);
} */

tau::CombinedImage* tau::Instance::getImage(std::string& img) {
    if (image_cache.contains(img)) return &image_cache.at(img);

    tau::CombinedImage image;
    image.img = loadColorTexture(img.c_str());

    vk::SamplerCreateInfo sci{};
    sci.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sci.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sci.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sci.anisotropyEnable = false;
    sci.minFilter = vk::Filter::eNearest;
    sci.magFilter = vk::Filter::eNearest;
    sci.unnormalizedCoordinates = false;
    sci.compareEnable = false;
    sci.compareOp = vk::CompareOp::eAlways;
    sci.mipmapMode = vk::SamplerMipmapMode::eNearest;
    sci.mipLodBias = 0.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    sci.borderColor = vk::BorderColor::eIntOpaqueBlack;

    image.sampler = device.createSampler(sci);

    image_cache[img] = std::move(image);

    return &image_cache[img];
}

tau::Instance::Instance() {
    current_instance = this;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(1600, 900, "hey", nullptr, nullptr);
    std::cerr << "hey\n";

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);

    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = "Hey";
    appInfo.pEngineName = "yo";
    appInfo.apiVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);

    vk::InstanceCreateInfo ici{};
    ici.pApplicationInfo = &appInfo;
    ici.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    ici.ppEnabledLayerNames = validationLayers.data();

    ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();

    std::cerr << "hey\n";
    instance = vk::raii::Instance(ctx, ici);

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback = debugCallback;
    std::cerr << "hey\n";

    debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance, createInfo);

    std::cerr << "hey\n";

    surface = createSurface();
    physicalDevice = createPhysicalDevice();
    device = createDevice();

    auto indices = queueFamilies(physicalDevice, surface);

    graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(indices.presentFamily.value(), 0);

    swapchain = createSwapchain();
    commandPool = createCommandPool();
    commandBuffers = createCommandBuffers();
    renderPass = createRenderPass();
    depthTexture = createDepthTexture();
    createFramebuffersForSwapchain(swapchain);

    vk::SemaphoreCreateInfo sci{};

    vk::FenceCreateInfo fci{};
    fci.flags = vk::FenceCreateFlagBits::eSignaled;

    // Sync Objects
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        imageAvailableSemaphores.push_back(device.createSemaphore(sci));
        renderFinishedSemaphores.push_back(device.createSemaphore(sci));
        inFlightFences.push_back(device.createFence(fci));
    }
}

tau::Instance::~Instance() {
    glfwDestroyWindow(window);
}

tau::Image tau::Instance::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlagBits properties, vk::ImageAspectFlagBits aspect) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = usage;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    Image img{};
    img.image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = img.image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    img.memory = device.allocateMemory(allocInfo);

    img.image.bindMemory(*img.memory, 0);

    img.view = createImageView(*img.image, format, aspect);

    return img;
}

#include <stb_image.h>

tau::Image tau::Instance::loadColorTexture(const char *path) {
    int x;
    int y;
    int channels;

    auto data =  stbi_load(path, &x, &y, &channels, STBI_rgb_alpha);

    auto image = createImage(x, y, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor);

    transitionImageLayout(*image.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::DeviceSize imageSize = x * y * STBI_rgb_alpha;

    auto[buffer, mem] = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto mapped = mem.mapMemory(0, imageSize);
    std::memcpy(mapped, data, imageSize);
    mem.unmapMemory();

    stbi_image_free(data);

    auto cmd = beginSingleCommand();

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1};

    cmd.copyBufferToImage(*buffer, *image.image, vk::ImageLayout::eTransferDstOptimal, { region });
    
    cmd.end();
    
    vk::CommandBuffer cb = *cmd;
    
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    
    graphicsQueue.submit({ submitInfo });
    graphicsQueue.waitIdle();

    transitionImageLayout(*image.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    return image;
}

void tau::Instance::createFramebuffersForSwapchain(Swapchain& swapchain) {
    for (size_t i = 0; i < swapchain.imageViews.size(); ++i) {
        vk::ImageView attachments[] = {
            *swapchain.imageViews[i],
            *depthTexture.view
        };

        vk::FramebufferCreateInfo createInfo{};
        createInfo.renderPass = *renderPass;
        createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
        createInfo.pAttachments = attachments;
        createInfo.width = swapchain.extent.width;
        createInfo.height = swapchain.extent.height;
        createInfo.layers = 1;

        swapchain.framebuffers.push_back(vk::raii::Framebuffer(device, createInfo));
    }
}

tau::Image tau::Instance::createDepthTexture() {
    vk::Format depthFormat = findDepthFormat(physicalDevice);

    Image depth = createImage(swapchain.extent.width, swapchain.extent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth);

    transitionImageLayout(*depth.image, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    return depth;
}

void tau::Swapchain::recreate(tau::Instance &instance) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(instance.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(instance.window, &width, &height);
        glfwWaitEvents();
    }

    instance.device.waitIdle();

    // Destroy Swapchain before recreating
    framebuffers.~vector();
    imageViews.~vector();
    images.~vector();
    swapchain.~SwapchainKHR();

    *this = instance.createSwapchain();
    instance.depthTexture = instance.createDepthTexture();
    instance.createFramebuffersForSwapchain(*this);
}
