#pragma once
#include <cstdint>
#include <map>
#include "dh/chunk.hpp"

namespace dh::chunk {

// Matches CHUNKS.md section 7.
enum class State : uint8_t {
    Unloaded  = 0,
    Ready     = 1,  // generated, in memory, not active
    Active    = 2,  // in use
    Dormant   = 3,  // cached, not active
    Unloading = 4,  // transient; not observable externally in v1
};

struct AddrLess {
    bool operator()(const coords::ChunkAddress& a,
                    const coords::ChunkAddress& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    }
};

class Manager {
public:
    Manager(uint64_t world_seed, uint16_t generation_version);

    // Generate if not present, else return cached. Marks the chunk Ready.
    // Returns nullptr only on allocation failure (never in practice).
    const Chunk* load(coords::ChunkAddress addr);

    // Fetch without generating. Returns nullptr if not loaded.
    const Chunk* get(coords::ChunkAddress addr) const;

    // Remove a chunk. No-op if not present.
    void unload(coords::ChunkAddress addr);

    // Load every chunk within Chebyshev distance <= radius of center.
    void load_around(coords::ChunkAddress center, int32_t radius);

    // Unload every chunk with Chebyshev distance > radius from center.
    void unload_beyond(coords::ChunkAddress center, int32_t radius);

    State state_of(coords::ChunkAddress addr) const;
    size_t size() const { return chunks_.size(); }

    uint64_t world_seed() const { return seed_; }
    uint16_t generation_version() const { return version_; }

private:
    uint64_t seed_;
    uint16_t version_;
    std::map<coords::ChunkAddress, Chunk, AddrLess> chunks_;
};

} // namespace dh::chunk