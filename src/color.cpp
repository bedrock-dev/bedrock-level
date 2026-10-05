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

        const std::vector<std::string> water_block_names{"water"};
        const std::vector<std::string> leaves_block_names{"leave", "leaf_litter", "vine"};
        const std::vector<std::string> grass_block_names{"grass"};

        using tint_kind = biome_tint_kind;

        tint_kind classify_tint(std::string_view name) {
            for (const auto& s : water_block_names) {
                if (name.find(s) != std::string_view::npos) return tint_kind::water;
            }
            for (const auto& s : leaves_block_names) {
                if (name.find(s) != std::string_view::npos && name.find("cherry") == std::string::npos) return tint_kind::leaves;
            }
            for (const auto& s : grass_block_names) {
                if (name.find(s) != std::string_view::npos) return tint_kind::grass;
            }
            return tint_kind::none;
        }

        struct string_hash {
            using is_transparent = void;

            size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }
            size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view(value)); }
        };

        struct string_equal {
            using is_transparent = void;

            bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
        };

        using string_color_map = std::unordered_map<std::string, bl::color, string_hash, string_equal>;
        using string_color_variant_map = std::unordered_map<std::string, string_color_map, string_hash, string_equal>;
        using string_id_map = std::unordered_map<std::string, int, string_hash, string_equal>;

        std::unordered_map<biome, bl::color> biome_water_map;
        std::unordered_map<biome, bl::color> biome_leave_map;
        std::unordered_map<biome, bl::color> biome_grass_map;

        bl::color default_water_color{63, 118, 228};
        bl::color default_leave_color{113, 167, 77};
        bl::color default_grass_color{142, 185, 113};

        std::unordered_map<biome, bl::color> biome_color_map;

        string_color_map single_block_color_map;
        string_color_variant_map multi_block_color_map;

        std::vector<std::string> block_id_to_names;
        string_id_map block_name_to_ids;

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

        bl::color read_hex_color(std::string_view text);

        bl::color read_rgb_color(const nlohmann::json& value) {
            bl::color c;
            if (value.is_string()) return read_hex_color(value.get<std::string>());
            if (!value.is_array() || value.size() < 3) return c;
            c.r = static_cast<uint8_t>(value[0].get<int>());
            c.g = static_cast<uint8_t>(value[1].get<int>());
            c.b = static_cast<uint8_t>(value[2].get<int>());
            return c;
        }

        [[nodiscard]] int hex_digit(char ch) {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
        }

        /// "#rrggbb" or "#rrggbbaa" with the leading '#' optional; RGB values default opaque.
        bl::color read_hex_color(std::string_view text) {
            const auto digits = (!text.empty() && text.front() == '#') ? text.substr(1) : text;
            if (digits.size() != 6 && digits.size() != 8) {
                LOG_F(ERROR, "Invalid color string '%.*s'", static_cast<int>(text.size()), text.data());
                return {};
            }
            uint8_t channels[4] = {0, 0, 0, 255};
            const size_t channel_count = digits.size() / 2;
            for (size_t i = 0; i < channel_count; i++) {
                const int hi = hex_digit(digits[i * 2]);
                const int lo = hex_digit(digits[i * 2 + 1]);
                if (hi < 0 || lo < 0) {
                    LOG_F(ERROR, "Invalid color string '%.*s'", static_cast<int>(text.size()), text.data());
                    return {};
                }
                channels[i] = static_cast<uint8_t>(hi * 16 + lo);
            }
            return {channels[0], channels[1], channels[2], channels[3]};
        }

        /// Accept hex colors and legacy four-element RGBA arrays.
        bl::color read_block_color(const nlohmann::json& value) {
            if (value.is_string()) return read_hex_color(value.get<std::string>());
            if (value.is_array() && value.size() >= 4) {
                bl::color c;
                c.r = value[0].get<uint8_t>();
                c.g = value[1].get<uint8_t>();
                c.b = value[2].get<uint8_t>();
                c.a = value[3].get<uint8_t>();
                return c;
            }
            LOG_F(ERROR, "Invalid block color entry: %s", value.dump().c_str());
            return {};
        }

        bl::color read_tint_color(const nlohmann::json& arr) { return read_rgb_color(arr); }

    }  // namespace

    color get_biome_color(bl::biome b) {
        auto it = biome_color_map.find(b);
        return it == biome_color_map.end() ? bl::color() : it->second;
    }

    color get_block_by_name_tag(std::string_view name, std::string_view tag) {
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
                LOG_F(ERROR, "Can not found color for block %.*s-%.*s", static_cast<int>(name.size()), name.data(),
                      static_cast<int>(tag.size()), tag.data());
            }
        }
        return {};
    }

    color get_block_color(std::string_view name, std::string_view tag) { return get_block_by_name_tag(name, tag); }

    biome_tint_kind block_biome_tint_kind(std::string_view name) {
        switch (classify_tint(name)) {
            case tint_kind::water:
                return biome_tint_kind::water;
            case tint_kind::leaves:
                return biome_tint_kind::leaves;
            case tint_kind::grass:
                return biome_tint_kind::grass;
            default:
                return biome_tint_kind::none;
        }
    }

    bool is_water_block(std::string_view name) { return block_biome_tint_kind(name) == biome_tint_kind::water; }

    bool is_leaves_block(std::string_view name) { return block_biome_tint_kind(name) == biome_tint_kind::leaves; }

    bool is_grass_block(std::string_view name) { return block_biome_tint_kind(name) == biome_tint_kind::grass; }

    color get_biome_tint_color(biome b, biome_tint_kind kind) {
        switch (kind) {
            case biome_tint_kind::water: {
                auto it = biome_water_map.find(b);
                return it == biome_water_map.end() ? default_water_color : it->second;
            }
            case biome_tint_kind::leaves: {
                auto it = biome_leave_map.find(b);
                return it == biome_leave_map.end() ? default_leave_color : it->second;
            }
            case biome_tint_kind::grass: {
                auto it = biome_grass_map.find(b);
                return it == biome_grass_map.end() ? default_grass_color : it->second;
            }
            default:
                return {255, 255, 255, 255};
        }
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

                if (value.contains("water")) {
                    auto c = read_tint_color(value["water"]);
                    biome_water_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_water_color = c;
                }

                if (value.contains("grass")) {
                    auto c = read_tint_color(value["grass"]);
                    biome_grass_map[static_cast<biome>(id)] = c;
                    if (key == "default") default_grass_color = c;
                }

                if (value.contains("leaves")) {
                    auto c = read_tint_color(value["leaves"]);
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
                    vec.emplace_back(tag, read_block_color(color));
                }
                if (vec.size() == 1) {
                    single_block_color_map[blockname] = vec.begin()->second;
                } else if (vec.size() > 1) {
                    for (const auto& pair : vec) {
                        multi_block_color_map[blockname][pair.first] = pair.second;
                    }
                }
            }

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

    int block_name_to_runtime_id(std::string_view name) {
        const auto key = strip_minecraft_prefix(name);
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

    bl::color blend_color_with_biome(std::string_view name, bl::color color, bl::biome b) {
        switch (classify_tint(name)) {
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
