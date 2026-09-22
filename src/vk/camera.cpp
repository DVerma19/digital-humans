#include "dh/vk/camera.hpp"
#include <cmath>

namespace dh::vk {

namespace {

constexpr float kPitchLimit = 1.55334f;   // 89 degrees

void forward_dir(const Camera& c, float& fx, float& fy, float& fz) {
    const float cp = std::cos(c.pitch);
    const float sp = std::sin(c.pitch);
    const float cy = std::cos(c.yaw);
    const float sy = std::sin(c.yaw);
    fx = sy * cp;
    fy = sp;
    fz = -cy * cp;
}

} // namespace

void Camera::look_at_world(float tx, float ty, float tz) {
    const float dx = tx - x;
    const float dy = ty - y;
    const float dz = tz - z;
    yaw   = std::atan2(dx, -dz);
    const float horiz = std::sqrt(dx*dx + dz*dz);
    pitch = std::atan2(dy, horiz);
}

Mat4 Camera::view() const {
    float fx, fy, fz;
    forward_dir(*this, fx, fy, fz);
    return look_at(x, y, z, x + fx, y + fy, z + fz, 0.0f, 1.0f, 0.0f);
}

void Camera::move(float forward_in, float right_in, float up_in, float dt) {
    // Horizontal movement uses yaw only; pitch does not tilt movement.
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float fwx = sy,  fwz = -cy;
    const float rgx = cy,  rgz =  sy;

    x += (fwx * forward_in + rgx * right_in) * speed * dt;
    z += (fwz * forward_in + rgz * right_in) * speed * dt;
    y += up_in * speed * dt;
}

void Camera::rotate(float dx_pixels, float dy_pixels) {
    yaw   += dx_pixels * sensitivity;
    pitch -= dy_pixels * sensitivity;
    if (pitch >  kPitchLimit) pitch =  kPitchLimit;
    if (pitch < -kPitchLimit) pitch = -kPitchLimit;
}

} // namespace dh::vk