#include "dh/vk/render_chunk.hpp"
#include "dh/chunk.hpp"
#include "dh/hydro_chunk.hpp"
#include "dh/biome.hpp"
#include <cmath>
#include <vector>

namespace dh::vk {

namespace {

// Emit one quad (two triangles) if the 4 corners are not all at the same
// height in a degenerate way. For a heightmap this is always true, so we
// skip this check; face culling for heightmaps is done by backface culling
// on the GPU.
void emit_quad(std::vector<uint32_t>& idx, uint32_t v00, uint32_t v10,
               uint32_t v01, uint32_t v11) {
    idx.push_back(v00); idx.push_back(v01); idx.push_back(v10);
    idx.push_back(v10); idx.push_back(v01); idx.push_back(v11);
}

void build_arrays(coords::ChunkAddress addr, uint64_t seed, uint16_t version,
                  const dh::hydro::BasinGrid* basin, uint16_t lod,
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

    dh::chunk::Chunk c;
    c.address = addr;
    c.generation_version = version;
    dh::chunk::generate(c, seed);
    if (basin) {
        dh::hydro::refine_chunk(c, seed, *basin);
    }
    dh::biome::classify_chunk(c, seed);

    // LOD: skip step cells. LOD 0 = every cell, LOD 1 = every 2nd cell, etc.
    const int32_t step = 1 << lod;
    const int32_t out_dim = (S + step - 1) / step;   // number of output vertices per side

    verts.clear();
    verts.reserve(static_cast<size_t>(out_dim) * out_dim * 9);
    const float base_x = static_cast<float>(addr.x * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));
    const float base_z = static_cast<float>(addr.z * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));

    auto sample = [&](int32_t hx, int32_t hz) -> float {
        if (hx < 0) hx = 0;
        if (hx >= H) hx = H - 1;
        if (hz < 0) hz = 0;
        if (hz >= H) hz = H - 1;
        return elev[static_cast<size_t>(hz) * H + hx];
    };

    for (int32_t oz = 0; oz < out_dim; ++oz) {
        for (int32_t ox = 0; ox < out_dim; ++ox) {
            const int32_t lx = ox * step;
            const int32_t lz = oz * step;
            const int32_t hx = lx + 1;
            const int32_t hz = lz + 1;

            // For normals at LOD > 0, sample at the LOD step size.
            const float hL = sample(hx - step, hz);
            const float hR = sample(hx + step, hz);
            const float hD = sample(hx, hz - step);
            const float hU = sample(hx, hz + step);

            float nx = (hL - hR) * 0.5f;
            float nz = (hD - hU) * 0.5f;
            float ny = 1.0f;
            const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= len; ny /= len; nz /= len;

            const int32_t o = std::min(lz, S - 1) * S + std::min(lx, S - 1);

            verts.push_back(base_x + static_cast<float>(lx));
            verts.push_back(c.elevation[o]);
            verts.push_back(base_z + static_cast<float>(lz));
            verts.push_back(nx);
            verts.push_back(ny);
            verts.push_back(nz);
            verts.push_back(c.river_flow[o]);
            verts.push_back(c.lake_depth[o]);
            verts.push_back(static_cast<float>(c.biome_id[o]));
        }
    }

    idx.clear();
    idx.reserve(static_cast<size_t>(out_dim - 1) * (out_dim - 1) * 6);
    for (int32_t oz = 0; oz < out_dim - 1; ++oz) {
        for (int32_t ox = 0; ox < out_dim - 1; ++ox) {
            const uint32_t v00 = static_cast<uint32_t>(oz * out_dim + ox);
            const uint32_t v10 = v00 + 1;
            const uint32_t v01 = v00 + static_cast<uint32_t>(out_dim);
            const uint32_t v11 = v01 + 1;
            emit_quad(idx, v00, v10, v01, v11);
        }
    }
}

} // namespace

MeshData build_mesh_data(coords::ChunkAddress addr,
                         uint64_t seed, uint16_t version,
                         const dh::hydro::BasinGrid* basin,
                         uint16_t lod) {
    MeshData md;
    md.addr = addr;
    build_arrays(addr, seed, version, basin, lod, md.vertices, md.indices);
    md.vertex_count = static_cast<uint32_t>(md.vertices.size() / 9);
    md.index_count  = static_cast<uint32_t>(md.indices.size());
    return md;
}

RenderChunk upload_mesh_data(VkDevice device, VkPhysicalDevice phys,
                             const MeshData& md) {
    RenderChunk rc;
    rc.addr = md.addr;
    rc.vertex = create_buffer(device, phys, md.vertices.size() * sizeof(float),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              md.vertices.data());
    rc.vertex.count = md.vertex_count;
    rc.index  = create_buffer(device, phys, md.indices.size() * sizeof(uint32_t),
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              md.indices.data());
    rc.index.count = md.index_count;
    return rc;
}

void destroy_render_chunk(VkDevice device, RenderChunk& rc) {
    destroy_buffer(device, rc.vertex);
    destroy_buffer(device, rc.index);
}

} // namespace dh::vk