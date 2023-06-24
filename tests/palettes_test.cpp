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
    auto data = bl::utils::read_file("./pts/0_0.pt.palette");
    EXPECT_TRUE(!data.empty());
    auto palettes = bl::palette::read_palette_to_end(data.data(), data.size());
    for (auto p : palettes) {
        EXPECT_TRUE(p);
        p->write(std::cout, 0);
    }
}

TEST(Palette, memoryFree) {
    using namespace bl::palette;
    auto* t = new compound_tag("key");
    t->value["x"] = new int_tag("name");
    delete t;
}
