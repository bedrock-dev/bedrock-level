//
// Created by xhy on 2023/6/18.
//
#include "color.h"

#include "data_3d.h"
#include "gtest/gtest.h"
TEST(Color, readColorPalette) {
    bl::init_biome_color_palette_from_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");
    auto color = bl::get_biome_color(bl::ocean);
}
