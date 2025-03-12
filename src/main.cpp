#include <iostream>

#include "instance.h"

struct ClickerProps {
    int initial_value = 0;
};

const auto Clicker = tau::component<ClickerProps>([](ClickerProps props) mutable {
    return tau::view{
        .layout = tau::Block{ .dimensions = { .x = 100_per, .y = 900_px }, .padding = { .x = 8_px, .y = 4_px }, .margin = { .x = 8_px } },
        .style = tau::Gradient{ .from = tau::rgb(128, 141, 143), .to = tau::rgb(47, 50, 51) } | tau::Border{ .corner_radius = 4, .width = 2, .color = tau::rgb(0, 202, 0) }
    }(
        tau::view{
            .layout = tau::Block{ .dimensions = { .x = 100_per, .y = 50_px } },
            .style = tau::Gradient{ .from = tau::blue, .to = tau::red }
        }() | tau::view{
            .layout = tau::Block{ .dimensions = { .x = 75_per, .y = 100_px } },
            .style = tau::Gradient{ .from = tau::red, .to = tau::green }
        }() | tau::view{
            .layout = tau::Block{ .dimensions = { .x = 50_per, .y = 100_px } },
            .style = tau::Gradient{ .from = tau::blue, .to = tau::red }
        }() | tau::view{
            .layout = tau::Block{ .dimensions = { .x = 25_per, .y = 82_px }, .margin = { .x = 16_px, .y = 16_px } },
            .style = tau::Gradient{ .from = tau::red, .to = tau::green }
        }() | tau::view{
            .layout = tau::Block{ .dimensions = { .x = 256_px, .y = 256_px } },
            .style = tau::ImageBG{ .src = "res/tau.png" } | tau::Border{ .corner_radius = 128, .width = 2, .color = tau::rgb(234, 0, 255) }
        }() | tau::span{}("Tau is sick af")
    );
});

int main() {
    glfwInit();

    tau::Instance instance;
    // Gradient::state.layout = Gradient::createDescriptorSetLayout(instance);
    // Gradient::state.pipe = instance.createPipeline(Gradient::vert, Gradient::frag);

    instance.render(Clicker({ .initial_value = 1 }));

    glfwTerminate();
}