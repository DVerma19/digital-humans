#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <vulkan/vulkan.h>
#include "dh/coords.hpp"
#include "dh/vk/render_chunk.hpp"
#include "dh/vk/thread_pool.hpp"
#include "dh/hydro.hpp"

namespace dh::vk {

struct AddrLess {
    bool operator()(const coords::ChunkAddress& a,
                    const coords::ChunkAddress& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    }
};

class ChunkStreamer {
public:
    ChunkStreamer(VkDevice device, VkPhysicalDevice phys,
                  uint64_t world_seed, uint16_t generation_version,
                  int32_t radius, const dh::hydro::BasinGrid* basin,
                  uint32_t worker_threads);
    ~ChunkStreamer();

    ChunkStreamer(const ChunkStreamer&) = delete;
    ChunkStreamer& operator=(const ChunkStreamer&) = delete;

    // Called once per frame on the main thread.
    void update(float cam_x, float cam_y, float cam_z, int max_uploads);

    const std::map<coords::ChunkAddress, RenderChunk, AddrLess>& chunks() const {
        return loaded_;
    }

    coords::ChunkAddress center() const { return center_; }
    int32_t radius() const { return radius_; }
    size_t loaded_count() const { return loaded_.size(); }
    size_t inflight_count() const { return in_flight_.size(); }
    size_t pending_count() const { return pending_.size(); }

private:
    void schedule_pending();
    void drain_completed(int max_uploads);

    VkDevice          device_;
    VkPhysicalDevice  phys_;
    uint64_t          seed_;
    uint16_t          version_;
    int32_t           radius_;
    const dh::hydro::BasinGrid* basin_;

    ThreadPool pool_;

    coords::ChunkAddress center_{0, 0};
    bool                 have_center_ = false;

    std::map<coords::ChunkAddress, RenderChunk, AddrLess> loaded_;
    std::vector<coords::ChunkAddress>                     pending_;
    std::set<coords::ChunkAddress, AddrLess>              in_flight_;
    bool                                                  pending_dirty_ = true;

    std::mutex               done_mtx_;
    std::vector<MeshData>    done_;
};

} // namespace dh::vk