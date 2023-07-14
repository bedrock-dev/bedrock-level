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
            //            BL_ERROR("unknown key: %s", name.c_str());

            return {};
        }
        return it->second;
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
                biome_color_map[static_cast<biome>(id)] = c;
                //                printf("%s  [%d] -> %u %u %u\n", key.c_str(), id, c.r, c.g, c.b);
            }

        } catch (std::exception&) {
            return false;
        }

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

}  // namespace bl
