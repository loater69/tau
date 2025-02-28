#include <iostream>

#include "instance.h"

int main() {
    glfwInit();

    Instance instance;
    // Gradient::state.layout = Gradient::createDescriptorSetLayout(instance);
    // Gradient::state.pipe = instance.createPipeline(Gradient::vert, Gradient::frag);

    // view v{ .type = gradient, .style = { .from = red, .to = blue } };

    instance.loop();

    glfwTerminate();
}