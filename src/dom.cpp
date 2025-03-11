#include "instance.h"
#include "dom.h"

/* vk::raii::DescriptorSetLayout Gradient::createDescriptorSetLayout(Instance &i) {
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
} */

void tau::Component::element::render(tau::Instance& instance, int current_frame, vk::raii::CommandBuffer& cmd) {
    if (!child) child = render_func();

    child->render(instance, current_frame, cmd);
}

void tau::text::element::render(Instance& instance, int current_frame, vk::raii::CommandBuffer& cmd) {
    
}

tau::elements tau::operator|(std::unique_ptr<element>&& left, std::unique_ptr<element>&& right) {
    elements els;
    els.push_back(std::move(left));
    els.push_back(std::move(right));

    return els;
}

tau::elements tau::operator|(elements&& left, std::unique_ptr<element>&& right) {
    left.push_back(std::move(right));

    return left;
}

void tau::ImageBG::init() {
    image = Instance::current_instance->getImage(src);
}

void tau::ImageBG::write_bindings(size_t &n, vk::raii::Device &device, vk::raii::DescriptorSet &set)
{
    vk::DescriptorImageInfo info{};
    info.sampler = *image->sampler;
    info.imageView = *image->img.view;

    vk::WriteDescriptorSet wds{};
    wds.dstSet = *set;
    wds.dstBinding = n;
    wds.dstArrayElement = 0;
    wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    wds.descriptorCount = 1;
    wds.pImageInfo = &info;

    device.updateDescriptorSets({ wds }, nullptr);
    ++n;
}

void tau::ImageBG::poolSizes(std::vector<vk::DescriptorPoolSize> &pss) {
    vk::DescriptorPoolSize ps;
    ps.descriptorCount = Instance::max_frames_in_flight;
    ps.type = vk::DescriptorType::eCombinedImageSampler;

    pss.push_back(ps);
}
