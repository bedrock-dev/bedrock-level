//
// Created by xhy on 2023/3/30.
//

#include <gtest/gtest.h>

#include "data_3d.h"
#include "utils.h"

TEST(Data3d, BasicRead) {
    auto data = bl::utils::read_file("../data/dumps/data3d/0_0.data3d");
    EXPECT_TRUE(data.size() > 512);
    bl::data_3d d3d{};
    d3d.load(data.data(), data.size());
}
