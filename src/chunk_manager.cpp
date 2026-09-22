#include "dh/chunk_manager.hpp"

namespace dh::chunk {

namespace {

inline int32_t chebyshev(coords::ChunkAddress a, coords::ChunkAddress b) {
    const int32_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const int32_t dz = a.z > b.z ? a.z - b.z : b.z - a.z;
    return dx > dz ? dx : dz;
}

} // namespace

Manager::Manager(uint64_t world_seed, uint16_t generation_version)
    : seed_(world_seed), version_(generation_version) {}

const Chunk* Manager::load(coords::ChunkAddress addr) {
    auto it = chunks_.find(addr);
    if (it != chunks_.end()) return &it->second;

    Chunk c;
    c.address            = addr;
    c.generation_version = version_;
    generate(c, seed_);

    auto [ins, ok] = chunks_.emplace(addr, std::move(c));
    (void)ok;
    return &ins->second;
}

const Chunk* Manager::get(coords::ChunkAddress addr) const {
    auto it = chunks_.find(addr);
    return it == chunks_.end() ? nullptr : &it->second;
}

void Manager::unload(coords::ChunkAddress addr) {
    chunks_.erase(addr);
}

void Manager::load_around(coords::ChunkAddress center, int32_t radius) {
    for (int32_t dz = -radius; dz <= radius; ++dz) {
        for (int32_t dx = -radius; dx <= radius; ++dx) {
            load({center.x + dx, center.z + dz});
        }
    }
}

void Manager::unload_beyond(coords::ChunkAddress center, int32_t radius) {
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        if (chebyshev(it->first, center) > radius) {
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

State Manager::state_of(coords::ChunkAddress addr) const {
    return chunks_.count(addr) ? State::Ready : State::Unloaded;
}

} // namespace dh::chunk