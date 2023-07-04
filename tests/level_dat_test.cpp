//
// Created by xhy on 2023/6/21.
//
#include "level_dat.h"

#include "gtest/gtest.h"
#include "utils.h"
TEST(LevelDat, Load) {
    const std::string path = R"(C:\Users\xhy\dev\bedrock-level\data\worlds\a\level.dat)";
    bl::level_dat dat;
    EXPECT_TRUE(dat.load_from_file(path));
    BL_LOGGER("Spawn position: %d %d %d", dat.spawn_position().x, dat.spawn_position().y,
              dat.spawn_position().z);
    BL_LOGGER("Level name: %s", dat.level_name().c_str());
    BL_LOGGER("Storage Version: %d", dat.storage_version());
}

TEST(LevelDat, MemoryFree) {
    const std::string path = R"(C:\Users\xhy\dev\bedrock-level\data\worlds\a\level.dat)";
    auto *dat = new bl::level_dat;
    EXPECT_TRUE(dat->load_from_file(path));
    delete dat;
}
