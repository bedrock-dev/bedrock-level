//
// Created by xhy on 2023/3/31.
//

#include "actor.h"

#include <iostream>

#include "palette.h"

namespace bl {

    bool actor::load(const byte_t *data, size_t len) {
        auto palettes = bl::palette::read_palette_to_end(data, len);
        BL_LOGGER("Total %zu palettes\n", palettes.size());
        for (auto &palette : palettes) {
            palette->write(std::cout, 0);
        }
        printf("----------------------\n");
        return true;
    }
}  // namespace bl
