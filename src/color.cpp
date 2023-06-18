//
// Created by xhy on 2023/6/18.
//

#include "color.h"

namespace bl {
    color get_biome_color(bl::biome b) {
        return {static_cast<uint8_t>(b), static_cast<uint8_t>(b), static_cast<uint8_t>(b)};
    }
    bool init_biome_color_palette_from_file(const std::string& filename) { return false; }
}  // namespace bl
