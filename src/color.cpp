//
// Created by xhy on 2023/6/18.
//

#include "color.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.h"
#include "json/json.hpp"
#include "stb/stb_image_write.h"
#include "utils.h"

namespace bl {
    namespace {

        // biome id -> water
        const std::vector<std::string> water_block_names{"water"};
        const std::vector<std::string> leaves_block_names{"leave"};
        const std::vector<std::string> grass_block_names{"grass"};

        // which biome tint applies to a block name
        enum class tint_kind : uint8_t { none, water, leaves, grass };

        tint_kind classify_tint(std::string_view name) {
            for (const auto& s : water_block_names) {
                if (name.find(s) != std::string_view::npos) return tint_kind::water;
            }
            for (const auto& s : leaves_block_names) {
                if (name.find(s) != std::string_view::npos) return tint_kind::leaves;
            }
            for (const auto& s : grass_block_names) {
                if (name.find(s) != std::string_view::npos) return tint_kind::grass;
            }
            return tint_kind::none;
        }

        // block names repeat heavily in a world, so classify each unique name once
        tint_kind get_tint_kind(const std::string& name) {
            static std::unordered_map<std::string, tint_kind> cache;
            auto it = cache.find(name);
            if (it != cache.end()) return it->second;
            auto kind = classify_tint(name);
            cache.emplace(name, kind);
            return kind;
        }

        std::unordered_map<biome, bl::color> biome_water_map;
        std::unordered_map<biome, bl::color> biome_leave_map;
        std::unordered_map<biome, bl::color> biome_grass_map;

        bl::color default_water_color{63, 118, 228};
        bl::color default_leave_color{113, 167, 77};
        bl::color default_grass_color{142, 185, 113};

        // biome id -> biome color
        std::unordered_map<biome, bl::color> biome_color_map;

        std::unordered_map<std::string, bl::color> single_block_color_map;
        std::unordered_map<std::string, std::unordered_map<std::string, bl::color>> multi_block_color_map;

        // block id -> name (without "minecraft:" prefix), built from the block color table
        std::vector<std::string> block_id_to_names;
        std::unordered_map<std::string, int> block_name_to_ids;

        std::string_view strip_minecraft_prefix(std::string_view name) {
            constexpr std::string_view prefix = "minecraft:";
            if (name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix) {
                return name.substr(prefix.size());
            }
            return name;
        }

        bl::color blend_with_biome(const std::unordered_map<bl::biome, bl::color>& map, bl::color gray, bl::color default_color,
                                   bl::biome b) {
            auto it = map.find(b);
            auto x = it == map.end() ? default_color : it->second;
            gray.r = static_cast<int>(gray.r / 255.0 * x.r);
            gray.g = static_cast<int>(gray.g / 255.0 * x.g);
            gray.b = static_cast<int>(gray.b / 255.0 * x.b);
            return gray;
        }

        bl::color read_rgb_color(const nlohmann::json& arr) {
            bl::color c;
            if (arr.size() < 3) return c;
            c.r = static_cast<uint8_t>(arr[0].get<int>());
            c.g = static_cast<uint8_t>(arr[1].get<int>());
            c.b = static_cast<uint8_t>(arr[2].get<int>());
            return c;
        }

    }  // namespace

    color get_biome_color(bl::biome b) {
        auto it = biome_color_map.find(b);
        return it == biome_color_map.end() ? bl::color() : it->second;
    }

    color get_block_by_name_tag(const std::string& name, const std::string& tag) {
        auto it1 = single_block_color_map.find(name);
        if (it1 != single_block_color_map.end()) {
            return it1->second;
        }
        auto it2 = multi_block_color_map.find(name);
        if (it2 != multi_block_color_map.end() && !it2->second.empty()) {
            auto& map = it2->second;
            if (tag.empty()) return map.begin()->second;
            for (auto& kv : map) {
                if (kv.first.find(tag) != std::string::npos) {
                    return kv.second;
                }
            }
            return map.begin()->second;
        }
        if (config::log_missing_block_color()) {
            if (name.find("element") == std::string::npos) {
                LOG_F(ERROR, "Can not found color for block %s-%s", name.c_str(), tag.c_str());
            }
        }
        return {};
    }

    std::string get_biome_name(biome b) {
        auto name = magic_enum::enum_name(b);
        return name.empty() ? "unknown" : std::string(name);
    }

    bool init_biome_color_palette_from_file(const std::string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                LOG_F(ERROR, "Can not open biome color file %s", filename.c_str());
                return false;
            }
            nlohmann::json j;
            f >> j;
            for (auto& [key, value] : j.items()) {
                int id = value["id"].get<int>();

                if (value.contains("rgb")) {
                    biome_color_map[static_cast<biome>(id)] = read_rgb_color(value["rgb"]);
                }

                // water
                if (value.contains("water")) {
                    auto c = read_rgb_color(value["water"]);
                    biome_water_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_water_color = c;
                }

                if (value.contains("grass")) {
                    auto c = read_rgb_color(value["grass"]);
                    biome_grass_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_grass_color = c;
                }

                if (value.contains("leaves")) {
                    auto c = read_rgb_color(value["leaves"]);
                    biome_leave_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_leave_color = c;
                }
            }
        } catch (std::exception&) {
            return false;
        }
        return true;
    }

    bool init_block_color_from_file(const std::string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                LOG_F(ERROR, "Can not open block color file %s", filename.c_str());
                return false;
            }
            nlohmann::json j;
            f >> j;

            std::vector<std::pair<std::string, bl::color>> vec;
            for (const auto& [blockname, value] : j.items()) {
                vec.clear();
                for (const auto& [tag, color] : value.items()) {
                    bl::color c;
                    c.r = color[0].get<uint8_t>();
                    c.g = color[1].get<uint8_t>();
                    c.b = color[2].get<uint8_t>();
                    c.a = color[3].get<uint8_t>();
                    vec.emplace_back(tag, c);
                }
                if (vec.size() == 1) {
                    single_block_color_map[blockname] = vec.begin()->second;
                } else if (vec.size() > 1) {
                    for (const auto& pair : vec) {
                        multi_block_color_map[blockname][pair.first] = pair.second;
                    }
                }
            }

            // build a stable block id -> name table (names stored without the "minecraft:" prefix)
            block_id_to_names.clear();
            block_name_to_ids.clear();
            for (const auto& [blockname, value] : j.items()) {
                std::string key(strip_minecraft_prefix(blockname));
                if (block_name_to_ids.count(key)) continue;
                block_name_to_ids.emplace(key, static_cast<int>(block_id_to_names.size()));
                block_id_to_names.push_back(std::move(key));
            }
        } catch (std::exception& e) {
            LOG_F(ERROR, "Can not parse block color file %s: %s", filename.c_str(), e.what());
            return false;
        }
        return true;
    }

    int block_name_to_runtime_id(const std::string& name) {
        std::string key(strip_minecraft_prefix(name));
        auto it = block_name_to_ids.find(key);
        return it == block_name_to_ids.end() ? -1 : it->second;
    }

    const std::string& block_runtime_id_to_name(int id) {
        static const std::string empty;
        return (id < 0 || id >= static_cast<int>(block_id_to_names.size())) ? empty : block_id_to_names[id];
    }

    std::string block_runtime_id_to_full_name(int id) {
        const auto& name = block_runtime_id_to_name(id);
        return name.empty() ? std::string() : "minecraft:" + name;
    }

    void export_image(const std::vector<std::vector<color>>& b, int ppi, const std::string& name) {
        if (b.empty() || b[0].empty()) {
            LOG_F(ERROR, "export_image: empty image data");
            return;
        }
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

    bl::color blend_color_with_biome(const std::string& name, bl::color color, bl::biome b) {
        switch (get_tint_kind(name)) {
            case tint_kind::water:
                return blend_with_biome(biome_water_map, color, default_water_color, b);
            case tint_kind::leaves:
                return blend_with_biome(biome_leave_map, color, default_leave_color, b);
            case tint_kind::grass:
                return blend_with_biome(biome_grass_map, color, default_grass_color, b);
            default:
                return color;
        }
    }

}  // namespace bl
