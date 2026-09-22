#include "dh/img_dump.hpp"
#include "dh/chunk.hpp"
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dh::img {

namespace {

float pick(const chunk::Chunk& c, int32_t lx, int32_t lz, Field f) {
    const int32_t i = lz * chunk::SIZE + lx;
    switch (f) {
        case Field::Elevation: return c.elevation[i];
        case Field::Roughness: return c.roughness[i];
    }
    return 0.0f;
}

void write_u16(std::ofstream& o, uint16_t v) {
    o.put(static_cast<char>(v & 0xff));
    o.put(static_cast<char>((v >> 8) & 0xff));
}

void write_u32(std::ofstream& o, uint32_t v) {
    o.put(static_cast<char>(v & 0xff));
    o.put(static_cast<char>((v >> 8) & 0xff));
    o.put(static_cast<char>((v >> 16) & 0xff));
    o.put(static_cast<char>((v >> 24) & 0xff));
}

} // namespace

bool dump_chunk_grid(const std::string& path,
                     uint64_t world_seed,
                     uint16_t generation_version,
                     coords::ChunkAddress origin,
                     int32_t size_x, int32_t size_z,
                     Field field) {
    if (size_x <= 0 || size_z <= 0) return false;

    const int32_t W = size_x * chunk::SIZE;
    const int32_t H = size_z * chunk::SIZE;

    std::vector<float> values(static_cast<size_t>(W) * H, 0.0f);

    float lo = 0.0f, hi = 0.0f;
    bool  first = true;

    for (int32_t cz = 0; cz < size_z; ++cz) {
        for (int32_t cx = 0; cx < size_x; ++cx) {
            chunk::Chunk c;
            c.address = {origin.x + cx, origin.z + cz};
            c.generation_version = generation_version;
            chunk::generate(c, world_seed);

            for (int32_t lz = 0; lz < chunk::SIZE; ++lz) {
                for (int32_t lx = 0; lx < chunk::SIZE; ++lx) {
                    const float v = pick(c, lx, lz, field);
                    const int32_t px = cx * chunk::SIZE + lx;
                    const int32_t py = cz * chunk::SIZE + lz;
                    values[static_cast<size_t>(py) * W + px] = v;
                    if (first) { lo = hi = v; first = false; }
                    else { lo = std::min(lo, v); hi = std::max(hi, v); }
                }
            }
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const uint32_t row_stride = ((static_cast<uint32_t>(W) * 3u + 3u) / 4u) * 4u;
    const uint32_t pixel_bytes = row_stride * static_cast<uint32_t>(H);
    const uint32_t file_size   = 54u + pixel_bytes;

    // BMP file header
    out.put('B'); out.put('M');
    write_u32(out, file_size);
    write_u16(out, 0); write_u16(out, 0);
    write_u32(out, 54);

    // DIB header
    write_u32(out, 40);
    write_u32(out, static_cast<uint32_t>(W));
    write_u32(out, static_cast<uint32_t>(H));
    write_u16(out, 1);
    write_u16(out, 24);
    write_u32(out, 0);
    write_u32(out, pixel_bytes);
    write_u32(out, 2835);
    write_u32(out, 2835);
    write_u32(out, 0);
    write_u32(out, 0);

    const float range = (hi > lo) ? (hi - lo) : 1.0f;
    std::vector<uint8_t> row(row_stride, 0);
    for (int32_t py = H - 1; py >= 0; --py) {
        std::fill(row.begin(), row.end(), 0);
        for (int32_t px = 0; px < W; ++px) {
            const float v = values[static_cast<size_t>(py) * W + px];
            const float t = (v - lo) / range;
            const uint8_t b = static_cast<uint8_t>(
                std::clamp(t * 255.0f, 0.0f, 255.0f));
            const uint32_t off = static_cast<uint32_t>(px) * 3u;
            row[off + 0] = b;
            row[off + 1] = b;
            row[off + 2] = b;
        }
        out.write(reinterpret_cast<const char*>(row.data()), row_stride);
    }
    return static_cast<bool>(out);
}

} // namespace dh::img