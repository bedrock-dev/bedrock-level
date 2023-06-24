//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <gtest/gtest.h>

#include "utils.h"

TEST(SubChunk, BasicRead) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\sub_chunks\0_-1_0.subchunk)");
    bl::sub_chunk sub_chunk;

    sub_chunk.load(data.data(), data.size());
    sub_chunk.dump_to_file(stdout);
}

TEST(SubChunk, FreeMemory) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\sub_chunks\0_-1_0.subchunk)");
    auto *chunk = new bl::sub_chunk();
    chunk->load(data.data(), data.size());
    delete chunk;
}
