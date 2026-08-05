//
// Created by xhy on 2023/6/18.
//
#include "color.h"

#include "data_3d.h"
#include "gtest/gtest.h"
TEST(Color, readColorPalette) { bl::init_biome_color_palette_from_file(R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)"); }
TEST(Color, readBlockPalette) { bl::init_block_color_from_file(R"(C:\Users\xhy\dev\bedrock-level\data\colors\block.json)"); }

TEST(Color, exportImage) {
    bl::init_biome_color_palette_from_file(R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");
    std::vector<std::vector<bl::biome>> b(40, std::vector<bl::biome>(60, bl::biome::cherry_groves));
    b[12][32] = bl::biome::ocean;
    std::vector<std::vector<bl::color>> c(40, std::vector<bl::color>(60, bl::color()));
    for (auto i = 0u; i < b.size(); i++) {
        for (auto j = 0u; j < b[0].size(); j++) {
            c[i][j] = bl::get_biome_color(b[i][j]);
        }
    }
    bl::export_image(c, 10, "a.png");
}

// water/leaves/grass names get biome-tinted; other blocks pass through unchanged.
TEST(Color, BlendWithBiomeClassification) {
    using namespace bl;
    const color white{255, 255, 255, 255};
    // a pure-white input must yield the default tint color (255/255*x == x)
    auto water = blend_color_with_biome("minecraft:water", white, biome::ocean);
    EXPECT_NE(water.hex(), white.hex()) << "water should be tinted";
    EXPECT_EQ(water.r, 63);  // default_water_color
    EXPECT_EQ(water.g, 118);
    EXPECT_EQ(water.b, 228);

    auto leaves = blend_color_with_biome("minecraft:leaves", white, biome::forest);
    EXPECT_EQ(leaves.r, 113);  // default_leave_color
    EXPECT_EQ(leaves.g, 167);
    EXPECT_EQ(leaves.b, 77);

    auto grass = blend_color_with_biome("minecraft:grass_block", white, biome::plains);
    EXPECT_EQ(grass.r, 142);  // default_grass_color
    EXPECT_EQ(grass.g, 185);
    EXPECT_EQ(grass.b, 113);

    // non-tinted blocks are returned untouched
    auto stone = blend_color_with_biome("minecraft:stone", white, biome::plains);
    EXPECT_EQ(stone.hex(), white.hex());
    auto air = blend_color_with_biome("minecraft:air", white, biome::plains);
    EXPECT_EQ(air.hex(), white.hex());
}

// the memoized classifier must be stable: same name -> same result every call.
TEST(Color, BlendMemoizationStable) {
    using namespace bl;
    const color white{255, 255, 255, 255};
    auto a = blend_color_with_biome("minecraft:leaves", white, biome::forest);
    auto b = blend_color_with_biome("minecraft:leaves", white, biome::swampland);
    auto c = blend_color_with_biome("minecraft:leaves", white, biome::taiga);
    // different biomes may give different blends, but a single call repeated must be identical
    auto a2 = blend_color_with_biome("minecraft:leaves", white, biome::forest);
    EXPECT_EQ(a.hex(), a2.hex());
    EXPECT_NE(a.hex(), white.hex());
    (void)b;
    (void)c;
}