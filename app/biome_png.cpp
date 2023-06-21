//
// Created by xhy on 2023/3/31.
//

#include <string>

#include "bedrock_level.h"
#include "color.h"
int main() {
    bl::init_biome_color_palette_from_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");
    //
    //    const std::string level =
    //        R"(C:\Users\xhy\AppData\Local\Packages\Microsoft.MinecraftUWP_8wekyb3d8bbwe\LocalState\games\com.mojang\minecraftWorlds\UJ2RZHWAAAA=)";

    const std::string level = R"(C:\Users\xhy\Desktop\b)";

    bl::bedrock_level l;
    l.open(level);
    if (!l.is_open()) {
        fprintf(stderr, "Can not open level");
        return -1;
    }
    if (l.size_in_chunk() == 0) {
        BL_ERROR("Empty world");
        return 0;
    }

    const int DIM = 0;

    auto [minP, maxP] = l.get_range(DIM);

    auto center = bl::chunk_pos{(minP.x + maxP.x) / 2, (minP.z + maxP.z) / 2, DIM};
    const int R = 50;
    //    minP = {center.x - R, center.z - R, DIM};
    //    maxP = {center.x + R, center.z + R, DIM};

    const int W = maxP.x - minP.x + 1;
    const int H = maxP.z - minP.z + 1;

    //    bl::chunk_pos ch{minP.x + (maxP.x - minP.x) / 4, minP.z + (maxP.z - minP.z) / 4, DIM};
    //    maxP = ch;

    BL_LOGGER("W = %d H = %d", W * 16, H * 16);
    std::vector<std::vector<bl::color>> cm(H * 16, std::vector<bl::color>(W * 16, bl::color()));

    for (int x = minP.x; x <= maxP.x; x++) {
        for (int z = minP.z; z <= maxP.z; z++) {
            auto *chunk = l.get_chunk({x, z, DIM});
            if (chunk) {
                //                计算区块其在图像中的位置
                auto sx = (x - minP.x) * 16;
                auto sz = (z - minP.z) * 16;
                for (int xx = 0; xx < 16; xx++) {
                    for (int zz = 0; zz < 16; zz++) {
                        cm[sz + zz][sx + xx] = bl::get_biome_color(chunk->get_top_biome(xx, zz));
                    }
                }
            }
        }
    }

    bl::export_image(cm, 1, "biome.png");
    return 0;
}