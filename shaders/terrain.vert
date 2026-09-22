#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform Push {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out float v_elevation;

void main() {
    v_normal    = in_normal;
    v_elevation = in_position.y;
    gl_Position = pc.mvp * vec4(in_position, 1.0);
}