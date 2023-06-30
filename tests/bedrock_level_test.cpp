//
// Created by xhy on 2023/4/1.
//
#include "bedrock_level.h"

#include <gtest/gtest.h>

#include "bedrock_key.h"
#include "chunk.h"
#include "sub_chunk.h"
#include "utils.h"

const std::string root = R"(C:\Users\xhy\dev\bedrock-level\data\worlds\a)";

TEST(BedrockLevel, ZeroChunkTest) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./sample"));
    // auto *ch = level.get_chunk({-1, -1, 2});
    auto cp = chunk_pos{-1, -1, 2};
    auto key = chunk_key{chunk_key::SubChunkTerrain, cp, 3}.to_raw();
    std::string data;
    level.db()->Get(leveldb::ReadOptions(), key, &data);

    utils::write_file("a.subchunk", data.data(), data.size());

    // sub_chunk c;
    // c.load(reinterpret_cast<uint8_t *>(data.data()), data.size());
}

TEST(BedrockLevel, ReadChunk) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./sample"));
    auto *ch = level.get_chunk({6, 0, 2});
    EXPECT_TRUE(ch);
    if (ch) {
        for (int i = 0; i < 64; i++) {
            auto block = ch->get_block(0, i, 0);
            std::cout << block.name << std::endl;
        }
    } else {
        BL_LOGGER("Can not find chunk");
    }
}

TEST(BedrockLevel, ReadBlock) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("../data/worlds/a"));
    for (int i = -64; i < 64; i++) {
        //        auto b = level.get_block({0, i, 0}, 0);
        //        printf("%d: %s\n", i, b.name.c_str());
    }
    printf("\n");
}

TEST(BedrockLevel, ReadHeight) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("../data/worlds/a"));
    auto *chunk = level.get_chunk({0, 0, 0});
    if (!chunk) {
        BL_ERROR("Can not load chunk");
        return;
    }
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            printf("%02d ", chunk->get_height(i, j));
        }
        printf("\n");
    }
}

// TEST(BedrockLevel, getRange) {
//     bl::bedrock_level level;
//     EXPECT_TRUE(level.open("../data/worlds/a"));
//     auto [mi, ma] = level.get_range(0);
//     BL_ERROR("%s -- %s", mi.to_string().c_str(), ma.to_string().c_str());
// }
TEST(BedrockLevel, CloseAndOpen) {
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(root));
    level.close();
    EXPECT_TRUE(level.open(root));
    level.close();
}
