//
// Created by xhy on 2023/3/31.
//

#include <gtest/gtest.h>

#include <fstream>
#include <nbt-cpp/nbt.hpp>

#include "palette.h"
#include "utils.h"

TEST(Palette, readPendingTick) {
    using namespace nbt;
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\bes\-1_-1.blockentity.palette)");
    EXPECT_TRUE(!data.empty());
    auto palettes = bl::palette::read_palette_to_end(data.data(), data.size());
    BL_LOGGER("Palette size is %d", palettes.size());
    palettes[1]->write(std::cout, 0);
    //    for (auto p : palettes) {
    //        EXPECT_TRUE(p);
    //    }
}

TEST(Palette, memoryFree) {
    using namespace bl::palette;
    auto* t = new compound_tag("key");
    t->value["x"] = new int_tag("name");
    delete t;
}

TEST(Palette, toRaw) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\actors\144115188092633088.palette)");
    int r = 0;
    auto* nbt = bl::palette::read_one_palette(data.data(), r);
    nbt->write(std::cout, 0);
    auto raw = nbt->to_raw();
    BL_LOGGER("Raw size is %zu, origin size is %zu", raw.size(), data.size());
}
