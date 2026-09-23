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
                             int32_t radius, const dh::hydro::BasinGrid* basin,
                             uint32_t worker_threads)
    : device_(device), phys_(phys),
      seed_(world_seed), version_(generation_version),
      radius_(radius), basin_(basin),
      pool_(worker_threads) {}

ChunkStreamer::~ChunkStreamer() {
    // Wait for workers to finish. ThreadPool destructor joins them, but we
    // must drain completed meshes first so nothing is dropped.
    for (auto& kv : loaded_) destroy_render_chunk(device_, kv.second);
    loaded_.clear();

    // Destroy any mesh data still in the completed queue.
    std::lock_guard<std::mutex> lock(done_mtx_);
    done_.clear();
}

void ChunkStreamer::schedule_pending() {
    if (!pending_dirty_) return;
    pending_.clear();

    for (int32_t dz = -radius_; dz <= radius_; ++dz) {
        for (int32_t dx = -radius_; dx <= radius_; ++dx) {
            coords::ChunkAddress a{center_.x + dx, center_.z + dz};
            if (loaded_.find(a) != loaded_.end()) continue;
            if (in_flight_.count(a) > 0) continue;
            pending_.push_back(a);
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

    // Submit all pending to workers. Workers run as many in parallel as we
    // have threads; the rest queue up.
    for (const auto& addr : pending_) {
        in_flight_.insert(addr);
        pool_.submit([this, addr] {
            MeshData md = build_mesh_data(addr, seed_, version_, basin_, 0);
            std::lock_guard<std::mutex> lock(done_mtx_);
            done_.push_back(std::move(md));
        });
    }
    pending_.clear();
}

void ChunkStreamer::drain_completed(int max_uploads) {
    std::vector<MeshData> batch;
    {
        std::lock_guard<std::mutex> lock(done_mtx_);
        if (done_.empty()) return;
        const size_t take = std::min(done_.size(), static_cast<size_t>(max_uploads));
        batch.reserve(take);
        for (size_t i = 0; i < take; ++i) {
            batch.push_back(std::move(done_[i]));
        }
        done_.erase(done_.begin(), done_.begin() + static_cast<long>(take));
    }

    for (auto& md : batch) {
        in_flight_.erase(md.addr);
        // If we've moved away and this is now outside radius, drop it.
        if (cheb(md.addr, center_) > radius_) continue;
        if (loaded_.find(md.addr) != loaded_.end()) continue;

        RenderChunk rc = upload_mesh_data(device_, phys_, md);
        loaded_.emplace(md.addr, std::move(rc));
    }
}

void ChunkStreamer::update(float cam_x, float cam_y, float cam_z, int max_uploads) {
    const auto new_center = coords::world_to_chunk({cam_x, cam_y, cam_z});
    if (!have_center_ || new_center.x != center_.x || new_center.z != center_.z) {
        center_        = new_center;
        have_center_   = true;
        pending_dirty_ = true;
    }

    // Evict far chunks.
    const int32_t evict_r = radius_ + 2;
    for (auto it = loaded_.begin(); it != loaded_.end(); ) {
        if (cheb(it->first, center_) > evict_r) {
            destroy_render_chunk(device_, it->second);
            it = loaded_.erase(it);
        } else {
            ++it;
        }
    }

    // Upload completed meshes (bounded per frame).
    drain_completed(max_uploads);

    // If we have idle workers, schedule more.
    if (pool_.pending() < pool_.thread_count() * 2) {
        schedule_pending();
    }
}

} // namespace dh::vk