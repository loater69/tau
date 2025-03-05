#include <iostream>

#include "instance.h"

struct ClickerProps {
    int initial_value = 0;
};

const auto Clicker = tau::component<ClickerProps>([](ClickerProps props) mutable {
    return tau::view{ .layout = tau::Flex{ .dimensions = { .x = 100_px }, .padding = { .x = 8_px, .y = 4_px } }, .style = tau::Border{ .corner_radius = 8, .width = 1, .color = tau::blue } }();
});

int main() {
    glfwInit();

    tau::Instance instance;
    // Gradient::state.layout = Gradient::createDescriptorSetLayout(instance);
    // Gradient::state.pipe = instance.createPipeline(Gradient::vert, Gradient::frag);

    tau::view v{ .layout = tau::Flow{ .dimensions = { .x = 5_per } }, .style = tau::Gradient{ .from = tau::red, .to = tau::blue } | tau::Border{ .corner_radius = 16, .width = 4, .color = tau::green } };

    std::cout << "code: " << v.style.compile() << '\n';

    instance.render(Clicker({ .initial_value = 1 }));

    glfwTerminate();
}