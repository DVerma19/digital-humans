#version 450

layout(location = 0) in vec3 in_position;

layout(push_constant) uniform Push {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 v_world;

void main() {
    v_world = in_position;
    gl_Position = pc.mvp * vec4(in_position, 1.0);
}