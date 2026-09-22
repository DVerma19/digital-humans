#version 450

layout(location = 0) in vec3  v_normal;
layout(location = 1) in float v_elevation;
layout(location = 2) in float v_river;
layout(location = 3) in float v_lake;
layout(location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(v_normal);

    vec3 light_dir = normalize(vec3(-0.5, 1.0, 0.6));
    float diff = max(dot(n, light_dir), 0.0);

    float slope = 1.0 - n.y;
    float ao = clamp(1.0 - slope * 0.7, 0.3, 1.0);

    float shade = (0.20 + 0.80 * diff) * ao;

    // Elevation-based ground palette.
    float t = clamp((v_elevation + 200.0) / 800.0, 0.0, 1.0);
    vec3 low  = vec3(0.25, 0.42, 0.20);
    vec3 mid  = vec3(0.58, 0.47, 0.32);
    vec3 high = vec3(0.92, 0.94, 0.96);
    vec3 ground = (t < 0.5)
        ? mix(low, mid, t * 2.0)
        : mix(mid, high, (t - 0.5) * 2.0);

    vec3 c = ground * shade;

    // Lakes: opaque blue, darken with depth.
    if (v_lake > 0.0) {
        float d = clamp(v_lake / 8.0, 0.0, 1.0);
        vec3 shallow = vec3(0.20, 0.50, 0.75);
        vec3 deep    = vec3(0.05, 0.15, 0.40);
        vec3 water   = mix(shallow, deep, d);
        c = mix(c, water, 0.85);
    }
    // Rivers: partial blue on top of ground.
    else if (v_river > 0.0) {
        vec3 river = vec3(0.25, 0.55, 0.80);
        c = mix(c, river, clamp(v_river, 0.0, 1.0) * 0.85);
    }

    out_color = vec4(c, 1.0);
}