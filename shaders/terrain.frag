#version 450

layout(location = 0) in vec3  v_normal;
layout(location = 1) in float v_elevation;
layout(location = 2) in float v_river;
layout(location = 3) in float v_lake;
layout(location = 4) flat in int v_biome;
layout(location = 0) out vec4 out_color;

vec3 biome_color(int b) {
    if (b ==  0) return vec3(0.10, 0.20, 0.45);   // OCEAN
    if (b ==  1) return vec3(0.15, 0.35, 0.60);   // LAKE
    if (b ==  2) return vec3(0.85, 0.80, 0.60);   // BEACH
    if (b ==  3) return vec3(0.85, 0.70, 0.40);   // DESERT
    if (b ==  4) return vec3(0.70, 0.65, 0.35);   // SAVANNA
    if (b ==  5) return vec3(0.15, 0.45, 0.15);   // TROPICAL_FOREST
    if (b ==  6) return vec3(0.55, 0.70, 0.35);   // GRASSLAND
    if (b ==  7) return vec3(0.30, 0.55, 0.25);   // TEMPERATE_FOREST
    if (b ==  8) return vec3(0.20, 0.40, 0.20);   // BOREAL_FOREST
    if (b ==  9) return vec3(0.55, 0.55, 0.45);   // TUNDRA
    if (b == 10) return vec3(0.95, 0.95, 0.98);   // SNOW
    if (b == 11) return vec3(0.50, 0.50, 0.50);   // MOUNTAIN
    if (b == 12) return vec3(0.35, 0.50, 0.30);   // WETLAND
    if (b == 13) return vec3(0.70, 0.65, 0.45);   // RIVERBANK
    return vec3(0.5, 0.5, 0.5);
}

void main() {
    vec3 n = normalize(v_normal);

    vec3 light_dir = normalize(vec3(-0.5, 1.0, 0.6));
    float diff = max(dot(n, light_dir), 0.0);
    float slope = 1.0 - n.y;
    float ao = clamp(1.0 - slope * 0.7, 0.3, 1.0);
    float shade = (0.20 + 0.80 * diff) * ao;

    vec3 ground = biome_color(v_biome);
    vec3 c = ground * shade;

    // Water overrides.
    if (v_lake > 0.0) {
        float d = clamp(v_lake / 8.0, 0.0, 1.0);
        vec3 shallow = vec3(0.20, 0.50, 0.75);
        vec3 deep    = vec3(0.05, 0.15, 0.40);
        c = mix(shallow, deep, d);
    } else if (v_river > 0.0) {
        vec3 river = vec3(0.25, 0.55, 0.80);
        c = mix(c, river, clamp(v_river, 0.0, 1.0) * 0.85);
    }

    out_color = vec4(c, 1.0);
}