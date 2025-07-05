#include <iostream>
#include <vector>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "color.h"

int main() {
    bl::init_block_color_palette_from_file("../data/colors/block_color.json");
    bl::init_block_color_palette_from_file("../data/colors/biome_color.json");
    const auto path =
        R"(C:\Users\xhy\AppData\Local\Packages\Microsoft.MinecraftUWP_8wekyb3d8bbwe\LocalState\games\com.mojang\minecraftWorlds\7Y3IlrVtMkQ=)";
    bl::bedrock_level level;
    if (!level.open(path)) {
        fprintf(stderr, "Can not open level %s", path);
        return -1;
    }

    auto minP = bl::chunk_pos{-3, -14, 0};
    auto maxP = bl::chunk_pos{-1, 1, 0};

    const int W = maxP.x - minP.x + 1;
    const int H = maxP.z - minP.z + 1;
    std::vector<std::vector<bl::color>> cm(H * 16, std::vector<bl::color>(W * 16, bl::color()));
    for (int x = minP.x; x <= maxP.x; x++) {
        for (int z = minP.z; z <= maxP.z; z++) {
            auto *chunk = level.get_chunk({x, z, 0});
            if (chunk) {
                auto sx = (x - minP.x) * 16;
                auto sz = (z - minP.z) * 16;
                for (int xx = 0; xx < 16; xx++) {
                    for (int zz = 0; zz < 16; zz++) {
                        auto top_y = chunk->get_height(xx, zz) - 1;
                        auto color = chunk->get_block_color(xx, top_y, zz);
                        cm[sz + zz][sx + xx] = color;
                    }
                }
            }
        }
    }
    bl::export_image(cm, 1, "terrain.png");
}