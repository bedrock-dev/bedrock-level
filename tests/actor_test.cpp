//
// Created by xhy on 2023/3/31.
//

#include "actor.h"

#include <gtest/gtest.h>

#include "utils.h"

TEST(Actor, BaicRead) {
    auto data = bl::utils::read_file("./dump/actors/1.palette");
    bl::actor actor;
    actor.load(data.data(), data.size());
}
