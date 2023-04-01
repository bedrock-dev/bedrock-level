//
// Created by xhy on 2023/4/1.
//
#include <gtest/gtest.h>
#include "bedrock_level.h"

TEST(BedrockLevel, TraverseKey) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./sample"));
    level.for_each_chunk_pos([&](const chunk_pos &cp) {
        std::cout << cp.to_string() << std::endl;
        auto *ch = level.get_chunk(cp);
        EXPECT_TRUE(ch);
    });

    //-1, -1, 2
}

#include "bedrock_key.h"
#include "sub_chunk.h"
#include "utils.h"

TEST(BedrockLevel, ZeroChunkTest) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./sample"));
    // auto *ch = level.get_chunk({-1, -1, 2});
    auto cp = chunk_pos{-1, -1, 2};
    auto key = chunk_key{chunk_key::SubChunkTerrain, cp, 3}.to_raw();
    std::string data;
    level.db()->Get(leveldb::ReadOptions(), key, &data);

    utils::write_file("a.subchunk", (uint8_t *) data.data(), data.size());

    // sub_chunk c;

    // c.load(reinterpret_cast<uint8_t *>(data.data()), data.size());
}