#pragma once
#include "dh/vk/math.hpp"

namespace dh::vk {

struct Frustum {
    // 6 planes, each as (a, b, c, d) with a*x + b*y + c*z + d = 0, normalized.
    float planes[6][4];

    bool contains_aabb(float min_x, float min_y, float min_z,
                       float max_x, float max_y, float max_z) const;
};

// Extract frustum planes from a view-projection matrix (row-major or
// column-major; this function assumes the same layout used by Mat4 in math.hpp).
Frustum extract_frustum(const Mat4& vp);

} // namespace dh::vk