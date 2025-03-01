#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;
layout(location = 1) in ivec2 dim;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;
}

void main() {
    vec4 border = vec4(1.0, 0.0, 0.0, 1.0);
    vec4 bg = vec4(0.0, 0.0, 1.0, 1.0);

    vec2 dim = vec2(1600.0, 900.0);

    float borderWidth = 0.01;

    float bw = 1.0;

    float d = roundedBoxSDF(uv - vec2(0.5), vec2(0.5), 0.2);
    float d2 = roundedBoxSDF(uv - vec2(0.5), vec2(0.5 - (borderWidth / 2.0)), 0.2);
    if (d > 0.0) discard;

    outColor = d2 > 0.0 ? border : bg; // vec4(uv, 0.0, 1.0);
}