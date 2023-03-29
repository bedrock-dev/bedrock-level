//
// Created by xhy on 2023/3/29.
//

#include "bit_tools.h"
#include "catch2/catch_amalgamated.hpp"

TEST_CASE("mask") {
    REQUIRE(bl::bits::mask(0, 7) == 0b11111111);
    REQUIRE(bl::bits::mask(0, 6) == 0b01111111);
    REQUIRE(bl::bits::mask(0, 5) == 0b00111111);
    REQUIRE(bl::bits::mask(1, 5) == 0b00111110);
    REQUIRE(bl::bits::mask(2, 5) == 0b00111100);
    REQUIRE(bl::bits::mask(3, 5) == 0b00111000);
}

TEST_CASE("restructureBytes") {
}