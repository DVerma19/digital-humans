#version 450

layout(location = 0) in vec3 v_world;
layout(location = 0) out vec4 out_color;

void main() {
    float t = clamp((v_world.y + 200.0) / 800.0, 0.0, 1.0);
    vec3 low  = vec3(0.20, 0.35, 0.20);
    vec3 mid  = vec3(0.55, 0.45, 0.30);
    vec3 high = vec3(0.90, 0.90, 0.92);
    vec3 c = (t < 0.5)
        ? mix(low, mid, t * 2.0)
        : mix(mid, high, (t - 0.5) * 2.0);
    out_color = vec4(c, 1.0);
}