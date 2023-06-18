//
// Created by xhy on 2023/6/18.
//

#include "color.h"

#include <fstream>
#include <unordered_map>

#include "json/json.hpp"
#include "magic-enum/magic_enum.hpp"
namespace bl {
    namespace {
        std::unordered_map<biome, bl::color> cm;
    }

    color get_biome_color(bl::biome b) {
        auto it = cm.find(b);
        return it == cm.end() ? bl::color() : it->second;

        return {static_cast<uint8_t>(b), static_cast<uint8_t>(b), static_cast<uint8_t>(b)};
    }
    bool init_biome_color_palette_from_file(const std::string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                BL_ERROR("Can not open file %s", filename.c_str());
                return false;
            }
            nlohmann::json j;
            f >> j;
            for (auto& [key, value] : j.items()) {
                auto rgb = value["rbg"].array();
                assert(rgb.size() == 3);
                color c;
                c.r = static_cast<uint8_t>(rgb[0]);
                c.g = static_cast<uint8_t>(rgb[1]);
                c.b = static_cast<uint8_t>(rgb[2]);
            }

        } catch (std::exception& e) {
            return false;
        }

        return true;
    }
}  // namespace bl
