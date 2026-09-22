#include "dh/vk/chunk_streamer.hpp"
#include <algorithm>
#include <cstdlib>

namespace dh::vk {

namespace {

inline int32_t cheb(coords::ChunkAddress a, coords::ChunkAddress b) {
    const int32_t dx = std::abs(a.x - b.x);
    const int32_t dz = std::abs(a.z - b.z);
    return dx > dz ? dx : dz;
}

} // namespace

ChunkStreamer::ChunkStreamer(VkDevice device, VkPhysicalDevice phys,
                             uint64_t world_seed, uint16_t generation_version,
                             int32_t radius, const dh::hydro::BasinGrid* basin)
    : device_(device), phys_(phys),
      seed_(world_seed), version_(generation_version),
      radius_(radius), basin_(basin) {}

ChunkStreamer::~ChunkStreamer() {
    for (auto& kv : loaded_) destroy_render_chunk(device_, kv.second);
    loaded_.clear();
}

void ChunkStreamer::update(float cam_x, float cam_y, float cam_z, int max_builds) {
    const auto new_center = coords::world_to_chunk({cam_x, cam_y, cam_z});
    if (!have_center_ || new_center.x != center_.x || new_center.z != center_.z) {
        center_        = new_center;
        have_center_   = true;
        pending_dirty_ = true;
    }

    const int32_t evict_r = radius_ + 2;
    for (auto it = loaded_.begin(); it != loaded_.end(); ) {
        if (cheb(it->first, center_) > evict_r) {
            destroy_render_chunk(device_, it->second);
            it = loaded_.erase(it);
        } else {
            ++it;
        }
    }

    if (pending_dirty_) {
        pending_.clear();
        for (int32_t dz = -radius_; dz <= radius_; ++dz) {
            for (int32_t dx = -radius_; dx <= radius_; ++dx) {
                coords::ChunkAddress a{center_.x + dx, center_.z + dz};
                if (loaded_.find(a) == loaded_.end()) pending_.push_back(a);
            }
        }
        std::sort(pending_.begin(), pending_.end(),
                  [&](const coords::ChunkAddress& a, const coords::ChunkAddress& b) {
                      const int32_t da = cheb(a, center_);
                      const int32_t db = cheb(b, center_);
                      if (da != db) return da < db;
                      if (a.x != b.x) return a.x < b.x;
                      return a.z < b.z;
                  });
        pending_dirty_ = false;
    }

    int built = 0;
    auto it = pending_.begin();
    while (built < max_builds && it != pending_.end()) {
        const coords::ChunkAddress a = *it;
        if (loaded_.find(a) != loaded_.end() || cheb(a, center_) > radius_) {
            it = pending_.erase(it);
            continue;
        }
        loaded_.emplace(a, create_render_chunk(device_, phys_, a, seed_, version_, basin_));
        it = pending_.erase(it);
        ++built;
    }
}

} // namespace dh::vk