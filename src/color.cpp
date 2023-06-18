//
// Created by xhy on 2023/6/18.
//

#include "color.h"

#include <fstream>
#include <unordered_map>

#include "json/json.hpp"
#include "magic-enum/magic_enum.hpp"
#include "stb/stb_image_write.h"
namespace bl {
    namespace {
        std::unordered_map<biome, bl::color> b2c;
        std::unordered_map<std::string, biome> s2b;
        std::unordered_map<biome, std::string> b2s;
    }  // namespace
    void init_enum_map() {
        //        magic_enum::enum_for_each<biome>([](biome val) {
        //        });
    }
    color get_biome_color(bl::biome b) {
        auto it = b2c.find(b);
        return it == b2c.end() ? bl::color() : it->second;
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
                auto rgb = value["rgb"];
                assert(rgb.size() == 3);
                color c;
                c.r = static_cast<uint8_t>(rgb[0].get<int>());
                c.g = static_cast<uint8_t>(rgb[1].get<int>());
                c.b = static_cast<uint8_t>(rgb[2].get<int>());
                int id = value["id"].get<int>();
                b2c[static_cast<biome>(id)] = c;
                //                printf("%s  [%d] -> %u %u %u\n", key.c_str(), id, c.r, c.g, c.b);
            }

        } catch (std::exception& e) {
            return false;
        }

        return true;
    }
    void export_image(const std::vector<std::vector<color>>& b, int ppi, const std::string& name) {
        const int c = 3;
        const int h = (int)b.size() * ppi;
        const int w = (int)b[0].size() * ppi;

        std::vector<unsigned char> data(c * w * h, 0);

        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                auto color = b[i / ppi][j / ppi];
                data[3 * (j + i * w)] = color.r;
                data[3 * (j + i * w) + 1] = color.g;
                data[3 * (j + i * w) + 2] = color.b;
            }
        }

        stbi_write_png(name.c_str(), w, h, c, data.data(), 0);
    }
}  // namespace bl
