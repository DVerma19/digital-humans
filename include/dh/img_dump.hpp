#pragma once
#include <cstdint>
#include <string>
#include "dh/coords.hpp"

namespace dh::img {

enum class Field { Elevation, Roughness };

// Dump a grid of chunks as a 24-bit BMP (grayscale-in-RGB).
// size_x, size_z are in chunks. Output is (size_x*64) x (size_z*64) pixels.
bool dump_chunk_grid(const std::string& path,
                     uint64_t world_seed,
                     uint16_t generation_version,
                     coords::ChunkAddress origin,
                     int32_t size_x, int32_t size_z,
                     Field field);

} // namespace dh::img