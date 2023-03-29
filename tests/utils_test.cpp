//
// Created by xhy on 2023/3/29.
//

#include "catch2/catch_amalgamated.hpp"

#include "utils.h"

TEST_CASE("Logger") {
    int a = 1;
    BL_ERROR("This is a error message with a  = %d", a);
    BL_LOGGER("This is a logger message with a  = %d", a);
}


TEST_CASE("Test") {
    REQUIRE(true);
    REQUIRE(1 + 1 == 2);
}