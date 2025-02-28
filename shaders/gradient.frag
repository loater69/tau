#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;
/* layout(set = 1, binding = 0) uniform Ubo {
    vec4 from;
    vec4 to;
} ubo; */

void main() {
    outColor = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0), uv.y);
}