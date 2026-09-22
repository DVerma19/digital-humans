#include "dh/hash.hpp"
#include <blake3.h>
#include <cstring>

namespace dh::hash {

Hash256 blake3(const void* data, std::size_t len) {
    Hash256 out{};
    blake3_hasher h;
    blake3_hasher_init(&h);
    if (len > 0) {
        blake3_hasher_update(&h, data, len);
    }
    blake3_hasher_finalize(&h, out.bytes, 32);
    return out;
}

uint64_t blake3_64(const void* data, std::size_t len) {
    Hash256 h = blake3(data, len);
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | h.bytes[i];
    }
    return v;
}

uint64_t field_seed(uint64_t world_seed, uint16_t generation_version,
                    uint16_t field_id) {
    uint8_t buf[12];
    for (int i = 0; i < 8; ++i) {
        buf[i] = static_cast<uint8_t>((world_seed >> (i * 8)) & 0xff);
    }
    buf[8]  = static_cast<uint8_t>(generation_version & 0xff);
    buf[9]  = static_cast<uint8_t>((generation_version >> 8) & 0xff);
    buf[10] = static_cast<uint8_t>(field_id & 0xff);
    buf[11] = static_cast<uint8_t>((field_id >> 8) & 0xff);
    return blake3_64(buf, sizeof(buf));
}

} // namespace dh::hash