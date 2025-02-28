#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;
}

void main() {
    vec2 dim = vec2(1600.0, 900.0);

    float d = roundedBoxSDF(uv - vec2(0.5), vec2(0.5), 0.2);
    if (d > 0.0) discard;

    outColor = vec4(uv, 0.0, 1.0);
}