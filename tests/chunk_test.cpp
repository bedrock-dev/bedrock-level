//
// Created by xhy on 2023/4/2.
//

#include "chunk.h"

#include <gtest/gtest.h>

void check_map(int y, int index, int offset) {
    int i, o;
    bl::chunk::map_y_to_subchunk(y, i, o);
    EXPECT_TRUE(i == index && o == offset);
}

TEST(Chunk, SubIndexMapping) {
    check_map(-64, -4, 0);
    check_map(-63, -4, 1);
    check_map(-49, -4, 15);
    check_map(-48, -3, 0);
    check_map(-47, -3, 1);
    check_map(-48, -3, 0);
    check_map(-16, -1, 0);
    check_map(-1, -1, 15);
    check_map(0, 0, 0);
    check_map(1, 0, 1);
    check_map(15, 0, 15);
    check_map(16, 1, 0);
}

TEST(Chunk, ValidInCHunkPos) {
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(15, 0, 15, -1));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(15, 0, 15, 3));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(15, 0, 16, 0));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(15, 0, -1, 0));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(16, 0, -1, 0));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(-1, 0, 16, 0));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(15, 0, 15, 0));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(15, 0, 15, 1));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(15, 0, 15, 2));
    // overworld
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, 320, 1, 0));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, -65, 1, 0));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, 319, 1, 0));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, -64, 1, 0));
    // nether
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, 128, 1, 1));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, -1, 1, 1));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, 127, 1, 1));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, 0, 1, 1));
    // the end
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, 256, 1, 2));
    EXPECT_FALSE(bl::chunk::valid_in_chunk_pos(1, -1, 1, 2));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, 255, 1, 2));
    EXPECT_TRUE(bl::chunk::valid_in_chunk_pos(1, 0, 1, 2));
}
