#pragma once

namespace dh::vk {

struct Mat4 { float m[16]; };

Mat4 identity();
Mat4 mul(const Mat4& a, const Mat4& b);

// Vulkan-correct projection: NDC Y-down, Z in [0, 1].
Mat4 perspective_vk(float fovy_rad, float aspect, float zn, float zf);

Mat4 look_at(float ex, float ey, float ez,
             float cx, float cy, float cz,
             float ux, float uy, float uz);

} // namespace dh::vk