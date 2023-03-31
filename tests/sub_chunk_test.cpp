//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <gtest/gtest.h>

#include "utils.h"

TEST(SubChunk, BasicRead) {
    auto data = bl::utils::read_file("../subchunks/0_0_0.subdata");
    bl::sub_chunk sub_chunk;
    sub_chunk.load(data.data(), data.size());
    sub_chunk.dump_to_file(stdout);
}
