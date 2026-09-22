#include "dh/img_dump.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    if (argc < 8) {
        std::fprintf(stderr,
            "usage: %s <seed> <version> <x> <z> <size_x> <size_z> <out.bmp> "
            "[elevation|roughness]\n", argv[0]);
        return 1;
    }

    const uint64_t seed    = std::strtoull(argv[1], nullptr, 10);
    const uint16_t version = static_cast<uint16_t>(
        std::strtoul(argv[2], nullptr, 10));
    const int32_t  x       = static_cast<int32_t>(
        std::strtol(argv[3], nullptr, 10));
    const int32_t  z       = static_cast<int32_t>(
        std::strtol(argv[4], nullptr, 10));
    const int32_t  sx      = static_cast<int32_t>(
        std::strtol(argv[5], nullptr, 10));
    const int32_t  sz      = static_cast<int32_t>(
        std::strtol(argv[6], nullptr, 10));
    const std::string out  = argv[7];

    dh::img::Field field = dh::img::Field::Elevation;
    if (argc >= 9) {
        if (std::strcmp(argv[8], "roughness") == 0)
            field = dh::img::Field::Roughness;
        else if (std::strcmp(argv[8], "elevation") != 0) {
            std::fprintf(stderr, "unknown field: %s\n", argv[8]);
            return 1;
        }
    }

    const bool ok = dh::img::dump_chunk_grid(
        out, seed, version, {x, z}, sx, sz, field);
    if (!ok) {
        std::fprintf(stderr, "failed to write %s\n", out.c_str());
        return 2;
    }

    std::printf("wrote %s (%d x %d pixels, field=%s)\n",
                out.c_str(), sx * 64, sz * 64,
                field == dh::img::Field::Elevation ? "elevation" : "roughness");
    return 0;
}