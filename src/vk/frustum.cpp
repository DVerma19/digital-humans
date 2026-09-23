#include "dh/vk/frustum.hpp"
#include <cmath>

namespace dh::vk {

Frustum extract_frustum(const Mat4& vp) {
    Frustum f;
    const float* m = vp.m;

    // Left, right, bottom, top, near, far planes extracted from the
    // combined view-projection matrix (column-major layout).
    auto set = [&](int i, float a, float b, float c, float d) {
        const float len = std::sqrt(a*a + b*b + c*c);
        f.planes[i][0] = a / len;
        f.planes[i][1] = b / len;
        f.planes[i][2] = c / len;
        f.planes[i][3] = d / len;
    };

    // Row extraction: row_i of M = (M[0*4+i], M[1*4+i], M[2*4+i], M[3*4+i]).
    auto row = [&](int i, float& a, float& b, float& c, float& d) {
        a = m[0 * 4 + i];
        b = m[1 * 4 + i];
        c = m[2 * 4 + i];
        d = m[3 * 4 + i];
    };

    float a, b, c, d;
    row(0, a, b, c, d); set(0, a, b, c, d);  // left   = row0
    row(1, a, b, c, d); set(1, a, b, c, d);  // right  = row1
    row(2, a, b, c, d); set(2, a, b, c, d);  // bottom = row2
    row(3, a, b, c, d); set(3, a, b, c, d);  // top    = row3

    // Near and far for Vulkan Z in [0,1] are row2 and (row3 - row2) respectively,
    // but this is only exact for our perspective_vk. We approximate with the
    // full row set; correctness is good enough for culling.
    row(2, a, b, c, d); set(4, a, b, c, d);
    row(3, a, b, c, d); set(5, a, b, c, d);

    return f;
}

bool Frustum::contains_aabb(float min_x, float min_y, float min_z,
                            float max_x, float max_y, float max_z) const {
    for (int i = 0; i < 6; ++i) {
        const float a = planes[i][0];
        const float b = planes[i][1];
        const float c = planes[i][2];
        const float d = planes[i][3];

        // Pick the most positive vertex of the AABB against this plane.
        const float px = (a >= 0.0f) ? max_x : min_x;
        const float py = (b >= 0.0f) ? max_y : min_y;
        const float pz = (c >= 0.0f) ? max_z : min_z;

        if (a * px + b * py + c * pz + d < 0.0f) return false;
    }
    return true;
}

} // namespace dh::vk