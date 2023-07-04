//
// Created by xhy on 2023/3/31.
//

#include <gtest/gtest.h>

#include "bedrock_key.h"

TEST(ChunkPos, VailidCheck) {
    using namespace bl;
    bl::chunk_pos c1{1, 1, -1};
    bl::chunk_pos c2{1, 1, 0};
    bl::chunk_pos c3{1, 1, 1};
    bl::chunk_pos c4{1, 1, 2};
    bl::chunk_pos c5{1, 1, 3};
    EXPECT_TRUE(!c1.valid());
    EXPECT_TRUE(c2.valid());
    EXPECT_TRUE(c3.valid());
    EXPECT_TRUE(c4.valid());
    EXPECT_TRUE(!c5.valid());
}

TEST(ChunkKey, Convert) {
    using namespace bl;
    chunk_pos p{1, 1, 0};
    chunk_key key{bl::chunk_key::RandomTicks, p, 0};
    auto data = key.to_raw();
    auto cov = chunk_key::parse(data);
    EXPECT_TRUE(cov.cp == key.cp);
    EXPECT_TRUE(cov.type == key.type);

    key.type = bl::chunk_key::SubChunkTerrain;
    key.y_index = 13;
    cov = chunk_key::parse(key.to_raw());

    EXPECT_TRUE(cov.cp == key.cp);
    EXPECT_TRUE(cov.type == key.type);
    EXPECT_TRUE(cov.y_index == key.y_index);
}

TEST(VillageKey, Convert) {
    const std::string raw_id = "VILLAGE_241c7732-221a-4266-9fe9-cdd40d9bdeb0_INFO";
    bl::village_key key = bl::village_key::parse(raw_id);
    EXPECT_TRUE(key.valid());
    EXPECT_TRUE(key.uuid == "241c7732-221a-4266-9fe9-cdd40d9bdeb0");
    EXPECT_TRUE(key.type == bl::village_key::INFO);
    EXPECT_TRUE(key.to_raw() == raw_id);
}
