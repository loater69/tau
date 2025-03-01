#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 dim;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;
}

void main() {
    vec4 border = vec4(1.0, 0.0, 0.0, 1.0);
    vec4 bg = vec4(0.0, 0.0, 1.0, 1.0);

    // vec2 dim = vec2(1600.0, 900.0);

    float border_width = 8.0;
    float border_radius = 30.0;

    vec2 size = dim;

    float d = roundedBoxSDF(uv * size - (size / 2.0), size / 2.0, border_radius);
    float d2 = roundedBoxSDF(uv * size - (size / 2.0), size / 2.0 - vec2(border_width), border_radius - border_width);
    if (d > 0.0) discard;

    outColor = d2 > 0.0 ? border : bg; // vec4(uv, 0.0, 1.0);
}