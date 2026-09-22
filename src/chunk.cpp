#include "dh/chunk.hpp"
#include "dh/noise.hpp"
#include <cstring>

namespace dh::chunk {

namespace {

constexpr double ELEV_BASE_AMP   = 400.0;
constexpr double ELEV_DETAIL_AMP =  30.0;

inline double world_x(coords::ChunkAddress addr, int32_t lx) {
    return static_cast<double>(addr.x) * coords::CHUNK_SIZE_XZ
         + static_cast<double>(lx) + 0.5;
}

inline double world_z(coords::ChunkAddress addr, int32_t lz) {
    return static_cast<double>(addr.z) * coords::CHUNK_SIZE_XZ
         + static_cast<double>(lz) + 0.5;
}

} // namespace

Chunk::Chunk()
    : elevation(CELL_COUNT, 0.0f),
      roughness(CELL_COUNT, 0.0f),
      river_flow(CELL_COUNT, 0.0f),
      lake_depth(CELL_COUNT, 0.0f) {}

float elevation_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version) {
    const double wx = world_x(addr, lx);
    const double wz = world_z(addr, lz);
    const double base = static_cast<double>(noise::sample_default(
        noise::FieldId::ELEVATION_BASE, wx, wz, world_seed, generation_version));
    const double detail = static_cast<double>(noise::sample_default(
        noise::FieldId::ELEVATION_DETAIL, wx, wz, world_seed, generation_version));
    return static_cast<float>(base * ELEV_BASE_AMP + detail * ELEV_DETAIL_AMP);
}

float roughness_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version) {
    const double wx = world_x(addr, lx);
    const double wz = world_z(addr, lz);
    const double v = static_cast<double>(noise::sample_default(
        noise::FieldId::ROUGHNESS, wx, wz, world_seed, generation_version));
    const double t = (v + 1.0) * 0.5;
    const double c = (t < 0.0) ? 0.0 : (t > 1.0) ? 1.0 : t;
    return static_cast<float>(c);
}

void generate(Chunk& c, uint64_t world_seed) {
    for (int32_t lz = 0; lz < SIZE; ++lz) {
        for (int32_t lx = 0; lx < SIZE; ++lx) {
            const int32_t i = lz * SIZE + lx;
            c.elevation[i] = elevation_at(c.address, lx, lz,
                                          world_seed, c.generation_version);
            c.roughness[i] = roughness_at(c.address, lx, lz,
                                          world_seed, c.generation_version);
        }
    }
}

hash::Hash256 hash_of(const Chunk& c) {
    constexpr std::size_t N = static_cast<std::size_t>(CELL_COUNT);
    std::vector<uint8_t> buf;
    buf.reserve(4 + 4 + 2 + N * 4 * 2);

    auto append_u32 = [&](uint32_t u) {
        for (int i = 0; i < 4; ++i)
            buf.push_back(static_cast<uint8_t>((u >> (i * 8)) & 0xff));
    };
    auto append_u16 = [&](uint16_t u) {
        for (int i = 0; i < 2; ++i)
            buf.push_back(static_cast<uint8_t>((u >> (i * 8)) & 0xff));
    };
    auto append_f32 = [&](float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        append_u32(u);
    };

    append_u32(static_cast<uint32_t>(c.address.x));
    append_u32(static_cast<uint32_t>(c.address.z));
    append_u16(c.generation_version);
    for (float f : c.elevation) append_f32(f);
    for (float f : c.roughness) append_f32(f);

    return hash::blake3(buf.data(), buf.size());
}

} // namespace dh::chunk