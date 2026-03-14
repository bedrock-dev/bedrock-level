//
// Created by xhy on 2023/6/18.
//

#ifndef BEDROCK_LEVEL_COLOR_H
#define BEDROCK_LEVEL_COLOR_H
#include <string>
#include <unordered_map>

#include "data_3d.h"
namespace bl {
    struct color {
        uint8_t r{0};
        uint8_t g{0};
        uint8_t b{0};
        uint8_t a{255};
        [[nodiscard]] inline int32_t hex() const {
            return (static_cast<int32_t>(r) << 24) | (static_cast<int32_t>(g) << 16) | (static_cast<int32_t>(b) << 6) |
                   static_cast<int32_t>(a);
        }
    };
    // name mapping
    std::string get_biome_name(biome b);

    // init
    bool init_biome_color_palette_from_file(const std::string& filename);
    bool init_block_color_from_file(const std::string& filename);

    // color calculation
    color get_biome_color(biome b);
    color get_block_by_name_tag(const std::string& name, const std::string& tag = {});
    bl::color blend_color_with_biome(const std::string& name, bl::color color, bl::biome b);
    // if true, the block missing color will be print to console
    void setUseColorDebugMode(bool enable);

    void export_image(const std::vector<std::vector<color>>& c, int ppi, const std::string& name);

}  // namespace bl

#endif  // BEDROCK_LEVEL_COLOR_H
