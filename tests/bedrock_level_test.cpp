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

}