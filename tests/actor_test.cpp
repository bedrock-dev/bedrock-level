//
// Created by xhy on 2023/3/31.
//

#include "actor.h"

#include <gtest/gtest.h>

#include "utils.h"

TEST(Actor, BaicRead) {
    auto data = bl::utils::read_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\dumps\actors\144115188092633088.palette)");
    bl::actor actor;
    actor.load(data.data(), data.size());
    actor.dump();
}
