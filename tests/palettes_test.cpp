//
// Created by xhy on 2023/3/31.
//

#include <gtest/gtest.h>

#include <fstream>

#include "palette.h"
#include "utils.h"

TEST(Palette, TagCopy) {
    using namespace bl::palette;
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\actors\144115188092633088.palette)");
    int r = 0;
    auto* nbt = bl::palette::read_one_palette(data.data(), r);

    auto nbt2 = *nbt;
    bl::palette::compound_tag nbt3(*nbt);
    auto* nbt4 = nbt->copy();

    std::stringstream f1;
    nbt->write(f1, 0);
    delete nbt;

    std::stringstream f2, f3, f4;
    nbt2.write(f2, 0);
    nbt3.write(f3, 0);
    nbt4->write(f4, 0);

    EXPECT_TRUE(f1.str() == f2.str() && f2.str() == f3.str() && f3.str() == f4.str());
}

TEST(Palette, MemoryFree) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\bes\-1_-1.blockentity.palette)");
    EXPECT_TRUE(!data.empty());
    auto palettes = bl::palette::read_palette_to_end(data.data(), data.size());
    BL_LOGGER("Palette size is %d", palettes.size());
    for (auto& p : palettes) {
        EXPECT_TRUE(p);
        delete p;
    }
}

TEST(Palete, ToRaw) {
    using namespace bl::palette;
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\actors\144115188092633088.palette)");
    int r = 0;
    auto* nbt = bl::palette::read_one_palette(data.data(), r);
    auto raw = nbt->to_raw();
    BL_LOGGER("raw size is %zu nbt data size is %zu", raw.size(), data.size());
    bl::utils::write_file("demo.nbt", raw.data(), raw.size());
}

TEST(Palete, ToRaw2) {
    using namespace bl::palette;
    auto data = bl::utils::read_file(R"(demo.nbt)");
    int r = 0;
    auto* nbt = bl::palette::read_one_palette(data.data(), r);
    nbt->write(std::cout, 0);
}

TEST(Palette, ReadOne) {
    using namespace bl::palette;
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\actors\144115188092633088.palette)");
    int r = 0;
    auto* nbt = bl::palette::read_one_palette(data.data(), r);
    nbt->write(std::cout, 0);
}
