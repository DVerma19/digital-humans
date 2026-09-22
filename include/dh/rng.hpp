#pragma once
#include <cstdint>

namespace dh::rng {

class PCG64 {
public:
    PCG64();
    PCG64(uint64_t seed, uint64_t sequence);

    void seed(uint64_t seed, uint64_t sequence);
    uint64_t next();

private:
    uint64_t state_lo_, state_hi_;
    uint64_t inc_lo_, inc_hi_;
};

// Create a PCG64 stream for a specific noise field.
// Implements NOISE.md section 3.
PCG64 field_stream(uint64_t world_seed, uint16_t generation_version,
                   uint16_t field_id);

} // namespace dh::rng