//
// Created by xhy on 2023/3/30.
//

#include <gtest/gtest.h>

#include "data_3d.h"
#include "utils.h"

TEST(Data3d, BasicRead) {
    auto data = bl::utils::read_file("./data3d/0_0.data3d");
    EXPECT_TRUE(data.size() > 512);
    bl::data_3d d3d{};
    d3d.load(data.data(), data.size());
    d3d.dump_to_file(stdout);
    EXPECT_TRUE(d3d.height(0, 0) == d3d.height_map()[0] - 64);
    EXPECT_TRUE(d3d.height(15, 15) == d3d.height_map()[255] - 64);
}
