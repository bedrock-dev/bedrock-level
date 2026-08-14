//
// Created by xhy on 2023/3/29.
//

#include <gtest/gtest.h>

#include <vector>

#include "palette.h"
#include "utils.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace {
    std::vector<byte_t> load_mcstructure() { return bl::utils::read_file(TEST_DATA_DIR "/mcstructures/test.mcstructure"); }
}  // namespace

TEST(McStructure, ParseTestFile) {
    auto raw = load_mcstructure();
    ASSERT_FALSE(raw.empty());

    auto s = bl::parse_mcstructure(raw.data(), raw.size());

    EXPECT_GT(s.size_x, 0);
    EXPECT_GT(s.size_y, 0);
    EXPECT_GT(s.size_z, 0);
    const auto expected = static_cast<int>(static_cast<size_t>(s.size_x) * s.size_y * s.size_z);
    EXPECT_EQ(expected, static_cast<int>(s.layers[0].size()));
    EXPECT_EQ(expected, static_cast<int>(s.layers[1].size()));

    // every index must be valid (-1 = void) or point into the palette
    EXPECT_FALSE(s.palette.empty());
    for (int idx : s.layers[0]) {
        EXPECT_GE(idx, -1);
        EXPECT_LT(idx, static_cast<int>(s.palette.size()));
    }
    for (int idx : s.layers[1]) {
        EXPECT_GE(idx, -1);
        EXPECT_LT(idx, static_cast<int>(s.palette.size()));
    }
}

TEST(McStructure, HandlesInvalidData) {
    const byte_t garbage[] = {0x01, 0x02, 0x03};
    auto s = bl::parse_mcstructure(garbage, sizeof(garbage));
    EXPECT_EQ(s.size_x, 0);
    EXPECT_EQ(s.size_y, 0);
    EXPECT_EQ(s.size_z, 0);
    EXPECT_TRUE(s.palette.empty());
    EXPECT_TRUE(s.layers[0].empty());
}

TEST(McStructure, DumpSummary) {
    auto raw = load_mcstructure();
    ASSERT_FALSE(raw.empty());
    auto s = bl::parse_mcstructure(raw.data(), raw.size());

    auto text = s.dump();
    EXPECT_NE(text.find("size="), std::string::npos);
    EXPECT_NE(text.find("palette ("), std::string::npos);
    EXPECT_NE(text.find("layer 0:"), std::string::npos);
    EXPECT_NE(text.find("entities:"), std::string::npos);
}
