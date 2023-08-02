//
// Created by xhy on 2023/6/18.
//

#include "color.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "json/json.hpp"
#include "palette.h"
#include "stb/stb_image_write.h"

namespace bl {
    namespace {
        // biome id -> water
        // 和群系有关的颜色白名单
        std::unordered_set<std::string> water_block_names;
        std::unordered_set<std::string> leaves_block_names;
        std::unordered_set<std::string> grass_block_names;

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

        bl::color blend_with_biome(const std::unordered_map<bl::biome, bl::color>& map,
                                   bl::color gray, bl::color default_color, bl::biome b) {
            auto it = map.find(b);
            auto x = it == map.end() ? default_color : it->second;
            gray.r = static_cast<int>(gray.r / 255.0 * x.r);
            gray.g = static_cast<int>(gray.g / 255.0 * x.g);
            gray.b = static_cast<int>(gray.b / 255.0 * x.b);
            return gray;
        }

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
            BL_LOGGER("Load json success: %s", filename.c_str());
            for (auto& item : j) {
                using namespace bl::palette;
                auto extra_data = item["extra_data"];
                auto block_name = item["name"].get<std::string>();

                if (extra_data.contains("use_grass_color") &&
                    extra_data["use_grass_color"].get<bool>()) {
                    grass_block_names.insert(block_name);
                }
                if (extra_data.contains("use_leaves_color") &&
                    extra_data["use_leaves_color"].get<bool>()) {
                    leaves_block_names.insert(block_name);
                }
                if (extra_data.contains("use_water_color") &&
                    extra_data["use_water_color"].get<bool>()) {
                    water_block_names.insert(block_name);
                }

                if (extra_data.contains("color")) {
                    auto rgb = extra_data["color"];
                    color c;
                    c.r = static_cast<uint8_t>(rgb[0].get<double>() * 255.0);
                    c.g = static_cast<uint8_t>(rgb[1].get<double>() * 255.0);
                    c.b = static_cast<uint8_t>(rgb[2].get<double>() * 255.0);
                    c.a = static_cast<uint8_t>(rgb[3].get<double>() * 255.0);

                    auto* root = new compound_tag("");
                    auto* name_key = new string_tag("name");
                    name_key->value = block_name;
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
        BL_LOGGER("Water blocks:");
        for (auto& b : water_block_names) {
            BL_LOGGER(" - %s", b.c_str());
        }
        BL_LOGGER("Leaves blocks:");
        for (auto& b : leaves_block_names) {
            BL_LOGGER(" - %s", b.c_str());
        }
        BL_LOGGER("Grass blocks:");
        for (auto& b : grass_block_names) {
            BL_LOGGER(" - %s", b.c_str());
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

    bl::color blend_color_with_biome(const std::string& name, bl::color color, bl::biome b) {
        if (water_block_names.count(name))
            return blend_with_biome(biome_water_map, color, default_water_color, b);
        if (grass_block_names.count(name))
            return blend_with_biome(biome_grass_map, color, default_grass_color, b);
        if (leaves_block_names.count(name))
            return blend_with_biome(biome_leave_map, color, default_leave_color, b);
        return color;
    }

}  // namespace bl
