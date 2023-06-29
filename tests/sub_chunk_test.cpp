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

TEST(SubChunk, LayerFree) {
    auto *l = new bl::sub_chunk::layer();
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\bes\-1_-1.blockentity.palette)");
    EXPECT_TRUE(!data.empty());
    auto palettes = bl::palette::read_palette_to_end(data.data(), data.size());
    BL_LOGGER("Palette size is %d", palettes.size());
    for (auto &p : palettes) {
        l->palettes.push_back(p);
    }
}

TEST(SubChunk, FreeMemory) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\sub_chunks\0_-1_0.subchunk)");
    auto *sub = new bl::sub_chunk();
    EXPECT_TRUE(sub);
    EXPECT_TRUE(sub->load(data.data(), data.size()));

    struct A {
        std::map<int, bl::sub_chunk *> subs;
        ~A() {
            for (auto &x : subs) delete x.second;
        }
    };
    auto *a = new A();
    a->subs[1] = sub;
    delete a;
}

TEST(SubChunk, GetBlock) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\sub_chunks\0_-1_0.subchunk)");
    auto *sub = new bl::sub_chunk();
    EXPECT_TRUE(sub);
    EXPECT_TRUE(sub->load(data.data(), data.size()));
    for (int i = 0; i < 16; i++) {
        auto raw = sub->get_block_raw(0, 0, i);
        raw->write(std::cout, 0);
    }
    delete sub;
}
