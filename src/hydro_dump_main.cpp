#include "dh/hydro.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

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

bool write_bmp(const std::string& path,
               const std::vector<uint8_t>& rgb, int W, int H) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const uint32_t row_stride = ((static_cast<uint32_t>(W) * 3u + 3u) / 4u) * 4u;
    const uint32_t pixel_bytes = row_stride * static_cast<uint32_t>(H);
    const uint32_t file_size = 54u + pixel_bytes;

    out.put('B'); out.put('M');
    write_u32(out, file_size);
    write_u16(out, 0); write_u16(out, 0);
    write_u32(out, 54);
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

    std::vector<uint8_t> row(row_stride, 0);
    for (int y = H - 1; y >= 0; --y) {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < W; ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 3;
            const uint32_t off = static_cast<uint32_t>(x) * 3u;
            row[off + 0] = rgb[i + 2];   // B
            row[off + 1] = rgb[i + 1];   // G
            row[off + 2] = rgb[i + 0];   // R
        }
        out.write(reinterpret_cast<const char*>(row.data()), row_stride);
    }
    return static_cast<bool>(out);
}

void render_accum(const dh::hydro::BasinGrid& g, std::vector<uint8_t>& rgb) {
    const int W = dh::hydro::BASIN_W;
    const int H = dh::hydro::BASIN_H;
    rgb.assign(static_cast<size_t>(W) * H * 3, 0);

    uint32_t max_accum = 1;
    for (const auto& c : g.cells) if (c.accum > max_accum) max_accum = c.accum;
    const float log_max = std::log(1.0f + static_cast<float>(max_accum));

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const auto& c = g.at(x, y);
            uint8_t r, gr, b;
            if (c.is_ocean) {
                r = 20; gr = 40; b = 120;
            } else if (c.is_lake) {
                r = 100; gr = 180; b = 220;
            } else {
                const float t = std::log(1.0f + static_cast<float>(c.accum)) / log_max;
                // ridges (t low): pale brown; channels (t high): deep blue
                r = static_cast<uint8_t>(200 * (1.0f - t) + 30  * t);
                gr= static_cast<uint8_t>(170 * (1.0f - t) + 70  * t);
                b = static_cast<uint8_t>(140 * (1.0f - t) + 200 * t);
            }
            const size_t i = (static_cast<size_t>(y) * W + x) * 3;
            rgb[i + 0] = r; rgb[i + 1] = gr; rgb[i + 2] = b;
        }
    }
}

void render_elevation(const dh::hydro::BasinGrid& g, std::vector<uint8_t>& rgb) {
    const int W = dh::hydro::BASIN_W;
    const int H = dh::hydro::BASIN_H;
    rgb.assign(static_cast<size_t>(W) * H * 3, 0);

    float lo = 1e30f, hi = -1e30f;
    for (const auto& c : g.cells) {
        if (c.elevation < lo) lo = c.elevation;
        if (c.elevation > hi) hi = c.elevation;
    }
    const float range = (hi > lo) ? (hi - lo) : 1.0f;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const auto& c = g.at(x, y);
            const float t = (c.elevation - lo) / range;
            const uint8_t v = static_cast<uint8_t>(std::clamp(t * 255.0f, 0.0f, 255.0f));
            const size_t i = (static_cast<size_t>(y) * W + x) * 3;
            rgb[i + 0] = v; rgb[i + 1] = v; rgb[i + 2] = v;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <seed> [version] [out_prefix]\n", argv[0]);
        return 1;
    }
    const uint64_t seed    = std::strtoull(argv[1], nullptr, 10);
    const uint16_t version = (argc >= 3)
        ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10))
        : 1;
    const std::string prefix = (argc >= 4) ? argv[3] : "basin";

    std::fprintf(stderr, "computing basin grid...\n");
    auto grid = dh::hydro::compute_basin_grid(seed, version);

    // Diagnostics
    int ocean = 0, lake = 0, flowing = 0;
    uint32_t max_accum = 1;
    uint8_t  max_order = 0;
    float    lo_e = 1e30f, hi_e = -1e30f;
    for (const auto& c : grid.cells) {
        if (c.is_ocean) ++ocean;
        else if (c.is_lake) ++lake;
        else ++flowing;
        if (c.accum > max_accum) max_accum = c.accum;
        if (c.order > max_order) max_order = c.order;
        if (c.elevation < lo_e) lo_e = c.elevation;
        if (c.elevation > hi_e) hi_e = c.elevation;
    }
    std::fprintf(stderr,
        "[basin] ocean=%d lake=%d flowing=%d  max_accum=%u max_order=%u  "
        "elev=[%.1f..%.1f]\n",
        ocean, lake, flowing, max_accum, max_order, lo_e, hi_e);

    std::vector<uint8_t> rgb;

    render_accum(grid, rgb);
    if (!write_bmp(prefix + "_accum.bmp", rgb, dh::hydro::BASIN_W, dh::hydro::BASIN_H)) {
        std::fprintf(stderr, "failed to write %s_accum.bmp\n", prefix.c_str());
        return 2;
    }
    render_elevation(grid, rgb);
    if (!write_bmp(prefix + "_elev.bmp", rgb, dh::hydro::BASIN_W, dh::hydro::BASIN_H)) {
        std::fprintf(stderr, "failed to write %s_elev.bmp\n", prefix.c_str());
        return 2;
    }

    std::fprintf(stderr, "wrote %s_accum.bmp and %s_elev.bmp (%dx%d)\n",
                 prefix.c_str(), prefix.c_str(),
                 dh::hydro::BASIN_W, dh::hydro::BASIN_H);
    return 0;
}