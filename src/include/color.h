#ifndef BEDROCK_LEVEL_COLOR_H
#define BEDROCK_LEVEL_COLOR_H
#include <cstdint>
#include <string>
#include <unordered_map>

#include "data_3d.h"
namespace bl {
    enum class biome_tint_kind : uint8_t { none, water, leaves, grass };
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
    std::string get_biome_name(biome b);

    bool init_biome_color_palette_from_file(const std::string& filename);
    bool init_block_color_from_file(const std::string& filename);

    // Block name/runtime ID tables are initialized once and then read-only.
    int block_name_to_runtime_id(const std::string& name);
    const std::string& block_runtime_id_to_name(int id);
    std::string block_runtime_id_to_full_name(int id);

    color get_biome_color(biome b);
    // Raw block colour lookup, without any biome tinting.
    color get_block_color(const std::string& name, const std::string& tag = {});
    color get_block_by_name_tag(const std::string& name, const std::string& tag = {});
    [[nodiscard]] biome_tint_kind block_biome_tint_kind(const std::string& name);
    [[nodiscard]] bool is_water_block(const std::string& name);
    [[nodiscard]] bool is_leaves_block(const std::string& name);
    [[nodiscard]] bool is_grass_block(const std::string& name);
    [[nodiscard]] color get_biome_tint_color(biome b, biome_tint_kind kind);
    bl::color blend_color_with_biome(const std::string& name, bl::color color, bl::biome b);

    void export_image(const std::vector<std::vector<color>>& c, int ppi, const std::string& name);

}  // namespace bl

#endif  // BEDROCK_LEVEL_COLOR_H
