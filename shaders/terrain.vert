#version 450

layout(location = 0) in vec3  in_position;
layout(location = 1) in vec3  in_normal;
layout(location = 2) in float in_river;
layout(location = 3) in float in_lake;
layout(location = 4) in float in_biome;

layout(push_constant) uniform Push {
    mat4 mvp;
} pc;

layout(location = 0) out vec3  v_normal;
layout(location = 1) out float v_elevation;
layout(location = 2) out float v_river;
layout(location = 3) out float v_lake;
layout(location = 4) flat out int v_biome;

void main() {
    v_normal    = in_normal;
    v_elevation = in_position.y;
    v_river     = in_river;
    v_lake      = in_lake;
    v_biome     = int(in_biome + 0.5);
    gl_Position = pc.mvp * vec4(in_position, 1.0);
}