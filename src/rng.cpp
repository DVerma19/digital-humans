#include "dh/rng.hpp"
#include "dh/hash.hpp"

namespace dh::rng {

namespace {

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
using u128 = unsigned __int128;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

constexpr u128 MULT =
    ((u128)0x2360ED051FC65DA4ULL << 64) | 0x4385DF649FCCF645ULL;

inline u128 pack(uint64_t hi, uint64_t lo) {
    return ((u128)hi << 64) | lo;
}

inline void unpack(u128 v, uint64_t& hi, uint64_t& lo) {
    hi = (uint64_t)(v >> 64);
    lo = (uint64_t)v;
}

} // namespace

PCG64::PCG64() : state_lo_(0), state_hi_(0), inc_lo_(1), inc_hi_(0) {}

PCG64::PCG64(uint64_t seed, uint64_t sequence) {
    this->seed(seed, sequence);
}

void PCG64::seed(uint64_t seed, uint64_t sequence) {
    state_lo_ = 0;
    state_hi_ = 0;
    inc_lo_ = (sequence << 1) | 1u;
    inc_hi_ = sequence >> 63;
    next();
    u128 s = pack(state_hi_, state_lo_);
    s += seed;
    unpack(s, state_hi_, state_lo_);
    next();
}

uint64_t PCG64::next() {
    u128 old = pack(state_hi_, state_lo_);
    u128 inc = pack(inc_hi_, inc_lo_);
    u128 s = old * MULT + inc;
    unpack(s, state_hi_, state_lo_);

    uint64_t rot   = (uint64_t)(old >> 122);
    uint64_t xored = (uint64_t)(old >> 64) ^ (uint64_t)old;
    return (xored >> rot) | (xored << ((-rot) & 63));
}

PCG64 field_stream(uint64_t world_seed, uint16_t generation_version,
                   uint16_t field_id) {
    uint64_t s = hash::field_seed(world_seed, generation_version, field_id);
    return PCG64(s, 0xda3e39cb94b95bdbULL);
}

} // namespace dh::rng