#pragma once
#include "dh/vk/math.hpp"

namespace dh::vk {

struct Camera {
    float x = 0, y = 0, z = 0;
    float yaw   = 0.0f;      // radians; 0 = looking down -Z
    float pitch = 0.0f;      // radians; + = up
    float speed       = 300.0f;   // world units per second
    float sensitivity = 0.002f;   // radians per pixel

    void look_at_world(float tx, float ty, float tz);

    void move(float forward, float right, float up, float dt);
    void rotate(float dx_pixels, float dy_pixels);

    Mat4 view() const;
};

} // namespace dh::vk