#include "dh/vk/math.hpp"
#include <cmath>

namespace dh::vk {

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k)
                s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

Mat4 perspective_vk(float fovy_rad, float aspect, float zn, float zf) {
    Mat4 r{};
    const float f = 1.0f / std::tan(fovy_rad * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = -f;
    r.m[10] = zf / (zn - zf);
    r.m[11] = -1.0f;
    r.m[14] = (zn * zf) / (zn - zf);
    return r;
}

Mat4 look_at(float ex, float ey, float ez,
             float cx, float cy, float cz,
             float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;

    float sx = fy*uz - fz*uy;
    float sy = fz*ux - fx*uz;
    float sz = fx*uy - fy*ux;
    float sl = std::sqrt(sx*sx + sy*sy + sz*sz);
    sx /= sl; sy /= sl; sz /= sl;

    float tx = sy*fz - sz*fy;
    float ty = sz*fx - sx*fz;
    float tz = sx*fy - sy*fx;

    Mat4 r = identity();
    r.m[0] = sx;  r.m[4] = sy;  r.m[8]  = sz;  r.m[12] = -(sx*ex + sy*ey + sz*ez);
    r.m[1] = tx;  r.m[5] = ty;  r.m[9]  = tz;  r.m[13] = -(tx*ex + ty*ey + tz*ez);
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz; r.m[14] = (fx*ex + fy*ey + fz*ez);
    return r;
}

} // namespace dh::vk