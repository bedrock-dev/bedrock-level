//
// Created by xhy on 2023/3/30.
//
#include "bedrock_level.h"

#include <gtest/gtest.h>

#include <iostream>

#include "bedrock_key.h"
#include "utils.h"

TEST(BedrockLevel, SimpleOpen) {
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
}

TEST(BedrockLevel, ExprotBlockEntity) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse_chunk_ley(it->key().ToString());
        printf("%s\n", k.to_string().c_str());
        if (k.type == chunk_key::BlockEntity) {
            utils::write_file(
                    "bes/" + std::to_string(k.x) + "_" + std::to_string(k.z) + "_blockentity.nbt",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}
