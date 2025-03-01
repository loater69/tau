#include <iostream>

#include "instance.h"

int main() {
    glfwInit();

    Instance instance;
    // Gradient::state.layout = Gradient::createDescriptorSetLayout(instance);
    // Gradient::state.pipe = instance.createPipeline(Gradient::vert, Gradient::frag);

    view v{ .style = Gradient{ .from = red, .to = blue } | Border{ .corner_radius = 16, .width = 4, .color = green } };

    std::cout << "code: " << v.style.compile() << '\n';

    instance.loop();

    glfwTerminate();
}