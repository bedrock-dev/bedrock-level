//
// Created by xhy on 2023/3/30.
//

#include <gtest/gtest.h>

#include "color.h"
#include "data_3d.h"
#include "utils.h"

TEST(Data3d, BasicRead) {
    auto data = bl::utils::read_file("../data/dumps/data3d/0_-1.data3d");
    EXPECT_TRUE(data.size() > 512);
    bl::biome3d d3d{};
    d3d.load_from_d3d(data.data(), data.size());
}

TEST(Data3d, MemoryFree) {
    auto data = bl::utils::read_file("../data/dumps/data3d/0_-1.data3d");
    EXPECT_TRUE(data.size() > 512);
    auto *d = new bl::biome3d;
    d->load_from_d3d(data.data(), data.size());
    delete d;
}

TEST(Data3d, BiomeRead) {
    bl::init_biome_color_palette_from_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");

    auto data = bl::utils::read_file("../data/dumps/data3d/0_0.data3d");
    EXPECT_TRUE(data.size() > 512);
    bl::biome3d d3d{};
    d3d.load_from_d3d(data.data(), data.size());

    for (int y = -64; y < 320; y++) {
        std::vector<std::vector<bl::color>> c(16, std::vector<bl::color>(16, bl::color()));
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                auto b = d3d.get_biome(x, y, z);
                c[x][z] = bl::get_biome_color(b);
            }
        }
        bl::export_image(c, 10, "./png/" + std::to_string(y + 64) + ".png");
    }
}

TEST(Data3d, TopBiomeRead) {
    bl::init_biome_color_palette_from_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");

    auto data = bl::utils::read_file("../data/dumps/data3d/1_-1.data3d");
    EXPECT_TRUE(data.size() > 512);
    bl::biome3d d3d{};
    d3d.load_from_d3d(data.data(), data.size());

    std::vector<std::vector<bl::color>> c(16, std::vector<bl::color>(16, bl::color()));
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            auto b = d3d.get_top_biome(x, z);
            c[x][z] = bl::get_biome_color(b);
        }
    }

    bl::export_image(c, 10, "surface.png");
}
