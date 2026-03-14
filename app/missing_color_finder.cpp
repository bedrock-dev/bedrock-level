#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "color.h"
#include "utils.h"

struct WorldInfo {
    std::string path;
    bl::block_pos p1;
    bl::block_pos p2;
};

void test_world(const WorldInfo& info) {
    bl::bedrock_level level;
    if (!level.open("worlds/" + info.path)) {
        BL_ERROR("Can not open level: %s", info.path.c_str());
    }

    BL_LOGGER("Level %s opened", info.path.c_str());
    auto cp1 = info.p1.to_chunk_pos();
    auto cp2 = info.p2.to_chunk_pos();
    auto minx = std::min(cp1.x, cp2.x);
    auto maxx = std::max(cp1.x, cp2.x);

    auto minz = std::min(cp1.z, cp2.z);
    auto maxz = std::max(cp1.z, cp2.z);

    BL_LOGGER("Range: [%d,%d] ~ [%d,%d]", minx, minz, maxx, maxz);

    const int W = maxx - minx + 1;
    const int H = maxz - minz + 1;
    std::vector<std::vector<bl::color>> cm(H * 16, std::vector<bl::color>(W * 16, bl::color()));
    for (int x = minx; x <= maxx; x++) {
        for (int z = minz; z <= maxz; z++) {
            auto* chunk = level.get_chunk({x, z, 0});
            if (chunk) {
                auto sx = (x - minx) * 16;
                auto sz = (z - minz) * 16;
                for (int xx = 0; xx < 16; xx++) {
                    for (int zz = 0; zz < 16; zz++) {
                        auto top = chunk->get_height(xx, zz) - 1;
                        auto info = chunk->get_block(xx, top, zz);
                        auto biome = chunk->get_biome(xx, top, zz);
                        auto color_with_biome = bl::blend_color_with_biome(info.name, info.color, biome);
                        cm[sz + zz][sx + xx] = color_with_biome;
                    }
                }
            }
        }
    }
    bl::export_image(cm, 1, info.path + ".png");
    level.close();
}

int main() {
    bl::setUseColorDebugMode(true);
    bl::init_block_color_from_file("data/colors/block_color.json");
    bl::init_biome_color_palette_from_file("data/colors/biome_color.json");
    std::vector<WorldInfo> test_worlds{{"116", {-176, 0, -32}, {15, 1, 47}}

    };
    for (const auto& world : test_worlds) {
        test_world(world);
    }
    return 0;
}