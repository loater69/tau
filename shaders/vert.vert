#version 450

struct Vertex {
    vec2 pos;
    vec2 uv;
};

layout(push_constant) uniform Trans {
    vec2 pos;
    vec2 scale;
    ivec2 dimensions;
} trans;

Vertex vertices[4] = {
    {{-1.0, -1.0}, {0.0, 0.0}},
    {{-1.0, 1.0}, {0.0, 1.0}},
    {{1.0, -1.0}, {1.0, 0.0}},
    {{1.0, 1.0}, {1.0, 1.0}}
};

int indices[6] = int[](
    0, 1, 2,
    1, 3, 2
);

layout(location = 0) out vec2 uv;
layout(location = 1) out ivec2 dim;

void main() {
    Vertex vertex = vertices[indices[gl_VertexIndex]];
    gl_Position = vec4(vertex.pos * trans.scale + trans.pos - vec2(0.5), 0.0, 1.0);
    uv = vertex.uv;
    dim = trans.dimensions * trans.scale;
}