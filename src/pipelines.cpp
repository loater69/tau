#include "instance.h"

#include <fstream>

vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device, const std::string& file) {
    std::ifstream str(file, std::ios::binary | std::ios::ate);
    auto s = str.tellg();
    str.seekg(0);

    std::vector<char> code(s);
    str.read(code.data(), s);

    str.close();

    vk::ShaderModuleCreateInfo smci{};
    smci.codeSize = code.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return vk::raii::ShaderModule(device, smci);
}

tau::Pipeline tau::Instance::createPipeline(const std::string& vert, const std::string& frag, std::span<vk::DescriptorSetLayout> sets) {
    auto vertModule = createShaderModule(device, vert);
    auto fragModule = createShaderModule(device, frag);

    vk::PipelineShaderStageCreateInfo stages[2];
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vertModule;
    stages[0].pName = "main";

    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *fragModule;
    stages[1].pName = "main";
    
    vk::PipelineVertexInputStateCreateInfo pvisci{};
    pvisci.vertexBindingDescriptionCount = 0;
    pvisci.vertexAttributeDescriptionCount = 0;

    vk::PipelineInputAssemblyStateCreateInfo piasci{};
    piasci.topology = vk::PrimitiveTopology::eTriangleList;
    piasci.primitiveRestartEnable = false;

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain.extent.width;
    viewport.height = (float)swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = swapchain.extent;

    vk::PipelineViewportStateCreateInfo pvsci{};
    pvsci.viewportCount = 1;
    pvsci.pViewports = &viewport;
    pvsci.scissorCount = 1;
    pvsci.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo prsci{};
    prsci.depthClampEnable = false;
    prsci.rasterizerDiscardEnable = false;
    
    prsci.polygonMode = vk::PolygonMode::eFill;
    prsci.lineWidth = 1.0f;

    prsci.cullMode = vk::CullModeFlagBits::eBack;
    prsci.frontFace = vk::FrontFace::eCounterClockwise;

    prsci.depthBiasEnable = false;

    vk::PipelineMultisampleStateCreateInfo pmsci{};
    pmsci.sampleShadingEnable = false;
    pmsci.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo pdssci{};
    pdssci.depthTestEnable = true;
    pdssci.depthWriteEnable = true;
    pdssci.depthCompareOp = vk::CompareOp::eLess;
    
    pdssci.depthBoundsTestEnable = false;

    vk::PipelineColorBlendAttachmentState pcbas{};
    pcbas.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    pcbas.blendEnable = true;
    pcbas.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    pcbas.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    pcbas.colorBlendOp = vk::BlendOp::eAdd;
    pcbas.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    pcbas.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    pcbas.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo pcbsci{};
    pcbsci.logicOpEnable = false;
    pcbsci.attachmentCount = 1;
    pcbsci.pAttachments = &pcbas;

    vk::DynamicState dynamicStates[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo pdsci{};
    pdsci.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    pdsci.pDynamicStates = dynamicStates;

    vk::PushConstantRange push_constant{};
    push_constant.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_constant.size = sizeof(tau::BoxConstants);

    vk::PipelineLayoutCreateInfo plci{};
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &push_constant;
    plci.setLayoutCount = sets.size();
    plci.pSetLayouts = sets.data();

    vk::raii::PipelineLayout layout(device, plci);

    vk::GraphicsPipelineCreateInfo gpci{};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &pvisci;
    gpci.pInputAssemblyState = &piasci;
    gpci.pViewportState = &pvsci;
    gpci.pRasterizationState = &prsci;
    gpci.pMultisampleState = &pmsci;
    gpci.pDepthStencilState = &pdssci;
    gpci.pColorBlendState = &pcbsci;
    gpci.pDynamicState = &pdsci;

    gpci.layout = *layout;

    gpci.subpass = 0;
    gpci.renderPass = *renderPass;

    return { .layout = std::move(layout), .pipeline = vk::raii::Pipeline(device, nullptr, gpci) };
}