//
// Created by xhy on 2023/3/30.
//

#include <catch2/catch_test_macros.hpp>
#include "utils.h"

TEST_CASE("LOAD") {
    auto data = bl::utils::read_file("./data3d/0_0.data3d");
    REQUIRE(data.size() > 01);
}
