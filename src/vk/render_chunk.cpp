#include "dh/vk/render_chunk.hpp"
#include "dh/chunk.hpp"
#include "dh/hydro_chunk.hpp"
#include <cmath>
#include <vector>

namespace dh::vk {

namespace {

void build_arrays(coords::ChunkAddress addr, uint64_t seed, uint16_t version,
                  const dh::hydro::BasinGrid* basin,
                  std::vector<float>& verts, std::vector<uint32_t>& idx) {
    constexpr int32_t S = dh::chunk::SIZE;
    constexpr int32_t H = S + 2;

    std::vector<float> elev(static_cast<size_t>(H) * H, 0.0f);
    for (int32_t hz = 0; hz < H; ++hz) {
        for (int32_t hx = 0; hx < H; ++hx) {
            elev[static_cast<size_t>(hz) * H + hx] =
                dh::chunk::elevation_at(addr, hx - 1, hz - 1, seed, version);
        }
    }

    // Water: generate chunk, refine if basin given.
    dh::chunk::Chunk c;
    c.address = addr;
    c.generation_version = version;
    dh::chunk::generate(c, seed);
    if (basin) {
        dh::hydro::refine_chunk(c, seed, *basin);
    }

    verts.clear();
    verts.reserve(static_cast<size_t>(S) * S * 8);
    const float base_x = static_cast<float>(addr.x * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));
    const float base_z = static_cast<float>(addr.z * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));

    auto sample = [&](int32_t hx, int32_t hz) -> float {
        return elev[static_cast<size_t>(hz) * H + hx];
    };

    for (int32_t lz = 0; lz < S; ++lz) {
        for (int32_t lx = 0; lx < S; ++lx) {
            const int32_t hx = lx + 1;
            const int32_t hz = lz + 1;

            const float hL = sample(hx - 1, hz);
            const float hR = sample(hx + 1, hz);
            const float hD = sample(hx, hz - 1);
            const float hU = sample(hx, hz + 1);

            float nx = (hL - hR) * 0.5f;
            float nz = (hD - hU) * 0.5f;
            float ny = 1.0f;
            const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= len; ny /= len; nz /= len;

            const int32_t o = lz * S + lx;
            const float river = c.river_flow[o];
            const float lake  = c.lake_depth[o];

            verts.push_back(base_x + static_cast<float>(lx));
            verts.push_back(sample(hx, hz));
            verts.push_back(base_z + static_cast<float>(lz));
            verts.push_back(nx);
            verts.push_back(ny);
            verts.push_back(nz);
            verts.push_back(river);
            verts.push_back(lake);
        }
    }

    idx.clear();
    idx.reserve(static_cast<size_t>(S - 1) * (S - 1) * 6);
    for (int32_t lz = 0; lz < S - 1; ++lz) {
        for (int32_t lx = 0; lx < S - 1; ++lx) {
            const uint32_t v00 = static_cast<uint32_t>(lz * S + lx);
            const uint32_t v10 = v00 + 1;
            const uint32_t v01 = v00 + static_cast<uint32_t>(S);
            const uint32_t v11 = v01 + 1;
            idx.push_back(v00); idx.push_back(v01); idx.push_back(v10);
            idx.push_back(v10); idx.push_back(v01); idx.push_back(v11);
        }
    }
}

} // namespace

RenderChunk create_render_chunk(VkDevice device, VkPhysicalDevice phys,
                                coords::ChunkAddress addr,
                                uint64_t seed, uint16_t version,
                                const dh::hydro::BasinGrid* basin) {
    std::vector<float>    verts;
    std::vector<uint32_t> idx;
    build_arrays(addr, seed, version, basin, verts, idx);

    RenderChunk rc;
    rc.addr   = addr;
    rc.vertex = create_buffer(device, phys, verts.size() * sizeof(float),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data());
    rc.vertex.count = static_cast<uint32_t>(verts.size() / 8);
    rc.index  = create_buffer(device, phys, idx.size() * sizeof(uint32_t),
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT, idx.data());
    rc.index.count = static_cast<uint32_t>(idx.size());
    return rc;
}

void destroy_render_chunk(VkDevice device, RenderChunk& rc) {
    destroy_buffer(device, rc.vertex);
    destroy_buffer(device, rc.index);
}

} // namespace dh::vk