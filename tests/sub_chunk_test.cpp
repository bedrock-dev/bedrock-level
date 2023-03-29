//
// Created by xhy on 2023/3/29.
//

#include "catch2/catch_amalgamated.hpp"
#include "sub_chunk.h"
#include "utils.h"

TEST_CASE("Read") {
    auto data = bl::utils::read_file("../subchunks/0_0_0.subdata");
    REQUIRE(!data.empty());
    bl::sub_chunk sub_chunk;
    sub_chunk.load(data.data(), data.size());
    sub_chunk.dump_to_file(stdout);
}
