#pragma once
#include <achievements/core.hpp>
#include "../src/core/rac2_save_format.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace fixtures {
using Save = std::array<std::uint8_t, achievements::save_size>;

inline void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

inline void word(std::uint8_t* destination, std::uint32_t value) {
    std::memcpy(destination, &value, sizeof(value));
}

inline void section(std::uint8_t* out, unsigned size,
                    const std::vector<std::pair<int, std::vector<std::uint8_t>>>& chunks) {
    auto* at = out + 8;
    for (const auto& [id, bytes] : chunks) {
        word(at, id);
        word(at + 4, static_cast<std::uint32_t>(bytes.size()));
        std::copy(bytes.begin(), bytes.end(), at + 8);
        at += 8 + ((bytes.size() + 3) & ~3);
    }
    word(at, 9000);  // Padding chunk, so packet dimensions match the native format.
    word(at + 4, static_cast<std::uint32_t>(out + size - 16 - at));
    word(out + size - 8, 0xffffffff);
    word(out, size - 8);
    word(out + 4, rac2_save_crc16(out + 8, size - 8));
}

inline Save save(unsigned level, unsigned ticks, unsigned wallet, unsigned challenge = 0,
                 bool vandal = false, bool complete = false, unsigned scored_bolts = 0) {
    Save result{};
    word(result.data(), RAC2_SAVE_GLOBAL_SIZE);
    word(result.data() + 4, RAC2_SAVE_LEVEL_SIZE);
    std::vector<std::uint8_t> loc(4), time(4), flags(205), skills(32), owned(56),
        records(80), progress(64), scenes(256), ship(4);
    word(loc.data(), level);
    word(time.data(), ticks);
    word(progress.data(), wallet);
    word(progress.data() + 8, complete ? 80 : 4);
    progress[0x12] = static_cast<std::uint8_t>(challenge);
    skills[15] = vandal;
    if (complete) {
        std::fill_n(skills.begin(), 30, 1);
        owned[43] = owned[44] = flags[70] = 1;
        scenes[0xa1 / 8] |= 1 << (0xa1 % 8);
        progress[0x20] = 4;
        word(ship.data(), 0xbb50);
    }
    section(result.data() + 8, RAC2_SAVE_GLOBAL_SIZE, {
        {0, loc}, {3, time}, {5, flags}, {8, skills}, {10, owned}, {30, records},
        {39, progress}, {43, scenes}, {44, ship},
    });
    for (unsigned i = 0; i < RAC2_SAVE_LEVELS; ++i) {
        const auto found = static_cast<std::uint8_t>(complete && i < 10);
        std::vector<std::uint8_t> score(4);
        word(score.data(), i == 0 ? scored_bolts * 3 / 4 : i == 1 ? scored_bolts - scored_bolts * 3 / 4 : 0);
        section(result.data() + 8 + RAC2_SAVE_GLOBAL_SIZE + i * RAC2_SAVE_LEVEL_SIZE,
                RAC2_SAVE_LEVEL_SIZE, {{3003, {found, found, found, found}}, {4008, score}});
    }
    return result;
}
} // namespace fixtures
