#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in float v_elevation;
layout(location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(v_normal);

    // Sun from the northwest.
    vec3 light_dir = normalize(vec3(-0.5, 1.0, 0.6));
    float diff = max(dot(n, light_dir), 0.0);

    // Cheap ambient occlusion by slope: flat areas bright, steep areas darker.
    float slope = 1.0 - n.y;
    float ao = clamp(1.0 - slope * 0.7, 0.3, 1.0);

    float shade = (0.20 + 0.80 * diff) * ao;

    // Elevation-based palette.
    float t = clamp((v_elevation + 200.0) / 800.0, 0.0, 1.0);
    vec3 low  = vec3(0.25, 0.42, 0.20);
    vec3 mid  = vec3(0.58, 0.47, 0.32);
    vec3 high = vec3(0.92, 0.94, 0.96);
    vec3 c = (t < 0.5)
        ? mix(low, mid, t * 2.0)
        : mix(mid, high, (t - 0.5) * 2.0);

    out_color = vec4(c * shade, 1.0);
}