//
// Created by xhy on 2023/6/18.
//

#include "color.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "json/json.hpp"
#include "palette.h"
#include "stb/stb_image_write.h"

namespace bl {
    namespace {
        // biome id -> water
        std::unordered_map<biome, bl::color> biome_water_map;
        std::unordered_map<biome, bl::color> biome_leave_map;
        std::unordered_map<biome, bl::color> biome_grass_map;

        bl::color default_water_color{63, 118, 228};
        bl::color default_leave_color{113, 167, 77};
        bl::color default_grass_color{142, 185, 113};

        //
        // biome id -> name
        std::unordered_map<biome, std::string> biome_id_map;
        // biome id -> biome color
        std::unordered_map<biome, bl::color> biome_color_map;

        // key 是palette的raw形态
        std::unordered_map<std::string, bl::color> block_color_map;

    }  // namespace

    color get_biome_color(bl::biome b) {
        auto it = biome_color_map.find(b);
        return it == biome_color_map.end() ? bl::color() : it->second;
    }
    color get_block_color_from_SNBT(const std::string& name) {
        auto it = block_color_map.find(name);
        if (it == block_color_map.end()) {
            return {};
        }
        return it->second;
    }

    std::string get_biome_name(biome b) {
        auto it = biome_id_map.find(b);
        return it == biome_id_map.end() ? "unknown" : it->second;
    }

    bool init_biome_color_palette_from_file(const std::string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                BL_ERROR("Can not open biome color file %s", filename.c_str());
                return false;
            }
            nlohmann::json j;
            f >> j;
            for (auto& [key, value] : j.items()) {
                int id = value["id"].get<int>();
                biome_id_map[static_cast<biome>(id)] = key;

                if (value.contains("rgb")) {
                    auto rgb = value["rgb"];
                    assert(rgb.size() == 3);
                    color c;
                    c.r = static_cast<uint8_t>(rgb[0].get<int>());
                    c.g = static_cast<uint8_t>(rgb[1].get<int>());
                    c.b = static_cast<uint8_t>(rgb[2].get<int>());
                    biome_color_map[static_cast<biome>(id)] = c;
                }

                // water
                if (value.contains("water")) {
                    auto water = value["water"];
                    assert(water.size() == 3);
                    color c;
                    c.r = static_cast<uint8_t>(water[0].get<int>());
                    c.g = static_cast<uint8_t>(water[1].get<int>());
                    c.b = static_cast<uint8_t>(water[2].get<int>());
                    biome_water_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_water_color = c;
                }

                if (value.contains("grass")) {
                    auto grass = value["grass"];
                    assert(grass.size() == 3);
                    color c;
                    c.r = static_cast<uint8_t>(grass[0].get<int>());
                    c.g = static_cast<uint8_t>(grass[1].get<int>());
                    c.b = static_cast<uint8_t>(grass[2].get<int>());
                    biome_grass_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_grass_color = c;
                }

                if (value.contains("leaves")) {
                    auto leaves = value["leaves"];
                    assert(leaves.size() == 3);
                    color c;
                    c.r = static_cast<uint8_t>(leaves[0].get<int>());
                    c.g = static_cast<uint8_t>(leaves[1].get<int>());
                    c.b = static_cast<uint8_t>(leaves[2].get<int>());
                    biome_leave_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_leave_color = c;
                }
            }

        } catch (std::exception&) {
            return false;
        }

        BL_LOGGER("Water color Map: %zu", biome_water_map.size());
        BL_LOGGER("Leaves color Map: %zu", biome_leave_map.size());
        BL_LOGGER("Grass color Map: %zu", biome_grass_map.size());
        return true;
    }

    bool init_block_color_palette_from_file(const std::string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                BL_ERROR("Can not open file %s", filename.c_str());
                return false;
            }
            nlohmann::json j;
            f >> j;
            for (auto& item : j) {
                using namespace bl::palette;
                //                std::cout << name << std::endl;
                auto extra_data = item["extra_data"];
                if (extra_data.contains("color")) {
                    auto rgb = extra_data["color"];
                    color c;
                    c.r = static_cast<uint8_t>(rgb[0].get<double>() * 255.0);
                    c.g = static_cast<uint8_t>(rgb[1].get<double>() * 255.0);
                    c.b = static_cast<uint8_t>(rgb[2].get<double>() * 255.0);
                    c.a = static_cast<uint8_t>(rgb[3].get<double>() * 255.0);

                    auto* root = new compound_tag("");
                    auto* name_key = new string_tag("name");
                    name_key->value = item["name"].get<std::string>();
                    auto* stat_tag = new compound_tag("states");
                    if (item.contains("states")) {
                        for (auto& [k, v] : item["states"].items()) {
                            if (v.type() == nlohmann::json::value_t::string) {
                                auto* t = new string_tag(k);
                                t->value = v.get<std::string>();
                                stat_tag->put(t);
                            } else if (v.type() == nlohmann::json::value_t::boolean) {
                                auto* t = new byte_tag(k);
                                t->value = v.get<bool>();
                                stat_tag->put(t);
                            } else if (v.type() == nlohmann::json::value_t::number_float) {
                                auto* t = new float_tag(k);
                                t->value = v.get<float>();
                                stat_tag->put(t);
                            } else if (v.type() == nlohmann::json::value_t::number_integer) {
                                auto* t = new int_tag(k);
                                t->value = v.get<int>();
                                stat_tag->put(t);

                            } else if (v.type() == nlohmann::json::value_t::number_unsigned) {
                                auto* t = new int_tag(k);
                                t->value = v.get<unsigned>();
                                stat_tag->put(t);
                            }
                        }
                    }
                    root->put(name_key);
                    root->put(stat_tag);

                    block_color_map[root->to_raw()] = c;
                    delete root;
                }
            }
        } catch (std::exception& e) {
            std::cout << "Err: " << e.what() << std::endl;
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
    std::unordered_map<std::string, bl::color>& get_block_color_table() { return block_color_map; }

    [[maybe_unused]] bl::color get_water_color(bl::color gray, bl::biome b) {
        auto it = biome_water_map.find(b);
        auto x = it == biome_water_map.end() ? default_water_color : it->second;
        gray.r = static_cast<int>(gray.r / 255.0 * x.r);
        gray.g = static_cast<int>(gray.g / 255.0 * x.g);
        gray.b = static_cast<int>(gray.b / 255.0 * x.b);
        return gray;
    }
}  // namespace bl
