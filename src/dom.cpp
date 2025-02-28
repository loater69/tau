#include "dom.h"

vk::raii::DescriptorSetLayout Gradient::createDescriptorSetLayout(Instance &i) {
    vk::DescriptorSetLayoutBinding dslb{};
    dslb.binding = 0;
    dslb.descriptorType = vk::DescriptorType::eUniformBuffer;

    vk::DescriptorSetLayoutCreateInfo dslci{};
    dslci.bindingCount = 1;
    dslci.pBindings = &dslb;

    return i.device.createDescriptorSetLayout(dslci);
}

vk::raii::DescriptorSet Gradient::createDescriptorSet(Instance &i)
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *i.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = 0;

    return std::move(i.device.allocateDescriptorSets(allocInfo)[0]);
}