//
// Created by xhy on 2023/3/31.
//

#include <gtest/gtest.h>
#include <nbt.hpp>
#include <fstream>
#include "utils.h"
#include "palette.h"

TEST(NBT, readBlockEntity) {
    using namespace nbt;
    auto data = bl::utils::read_file("./bes/0_8_blockentity.nbt");
    EXPECT_TRUE(!data.empty());
    auto palettes = bl::palette::read_palette_to_end(data.data(), data.size());
    for (auto p: palettes) {
        EXPECT_TRUE(p);
        p->write(std::cout, 0);
    }
}