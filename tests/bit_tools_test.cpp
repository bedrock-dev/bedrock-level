//
// Created by xhy on 2023/3/29.
//

#include <gtest/gtest.h>
//

// TEST(BitTool, Mask) {
//     EXPECT_EQ(bl::bits::mask(0, 7), 0b11111111);
//     EXPECT_EQ(bl::bits::mask(0, 6), 0b01111111);
//     EXPECT_EQ(bl::bits::mask(0, 5), 0b00111111);
//     EXPECT_EQ(bl::bits::mask(1, 5), 0b00111110);
//     EXPECT_EQ(bl::bits::mask(2, 5), 0b00111100);
//     EXPECT_EQ(bl::bits::mask(3, 5), 0b00111000);
// }
//
// TEST(Rearrange, Bit1) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(1, data, 4);
//     EXPECT_TRUE(res.size() == 32);
//     for (auto i : res) {
//         EXPECT_TRUE(i == 1);
//     }
//     byte_t data2[] = {0, 0, 0, 0};
//     auto res2 = bl::bits::rearrange_words(1, data2, 4);
//     EXPECT_TRUE(res2.size() == 32);
//     for (auto i : res2) {
//         EXPECT_TRUE(i == 0);
//     }
// }
//
// TEST(Rearrange, Bit2) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(2, data, 4);
//     EXPECT_TRUE(res.size() == 16);
//     for (auto i : res) {
//         EXPECT_TRUE(i == 3);
//     }
//     byte_t data2[] = {0, 0, 0, 0};
//     auto res2 = bl::bits::rearrange_words(2, data2, 4);
//     EXPECT_TRUE(res2.size() == 16);
//     for (auto i : res2) {
//         EXPECT_TRUE(i == 0);
//     }
//
//     auto v3 = static_cast<int8_t>(0b10101010);
//     byte_t data3[] = {v3, v3, v3, v3};
//     auto res3 = bl::bits::rearrange_words(2, data3, 4);
//     EXPECT_TRUE(res2.size() == 16);
//     for (auto i : res3) {
//         EXPECT_TRUE(i == 2);
//     }
// }
//
// TEST(Rearrange, Bit4) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(4, data, 4);
//     EXPECT_TRUE(res.size() == 8);
//     for (auto i : res) {
//         EXPECT_TRUE(i == 15);
//     }
//
//     byte_t data2[] = {0, 0, 0, 0};
//     auto res2 = bl::bits::rearrange_words(4, data2, 4);
//     EXPECT_TRUE(res2.size() == 8);
//     for (auto i : res2) {
//         EXPECT_TRUE(i == 0);
//     }
//
//     auto v3 = static_cast<int8_t>(0b10101010);
//     byte_t data3[] = {v3, v3, v3, v3};
//     auto res3 = bl::bits::rearrange_words(4, data3, 4);
//     EXPECT_TRUE(res2.size() == 8);
//     for (auto i : res3) {
//         EXPECT_TRUE(i == 10);
//     }
// }
//
// TEST(Rearrange, Bit16) {
//     auto v3 = static_cast<int8_t>(0b10101010);
//     byte_t data[] = {v3, v3, v3, v3};
//     auto res = bl::bits::rearrange_words(16, data, 4);
//     EXPECT_TRUE(res.size() == 2);
//     for (auto i : res) {
//         EXPECT_TRUE(i == 0xaaaa);
//     }
// }
//// 3 5 6
// TEST(Rearrange, Bit3) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(3, data, 4);
//     EXPECT_TRUE(res.size() == 10);
//     for (auto i : res) {
//         // BL_LOGGER("%d", i);
//         EXPECT_TRUE(i == 7);
//     }
// }
//
// TEST(Rearrange, Bit5) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(5, data, 4);
//     EXPECT_TRUE(res.size() == 6);
//     for (auto i : res) {
//         // BL_LOGGER("%d", i);
//         EXPECT_TRUE(i == 31);
//     }
// }
//
// TEST(Rearrange, Bit6) {
//     byte_t data[] = {-1, -1, -1, -1};
//     auto res = bl::bits::rearrange_words(6, data, 4);
//     EXPECT_TRUE(res.size() == 5);
//     for (auto i : res) {
//         // BL_LOGGER("%d", i);
//         EXPECT_TRUE(i == 63);
//     }
// }
//
// TEST(Rearrange, Custom) {
//     byte_t data[] = {0x00, 0x11, 0x11, 0x11};
//     auto res = bl::bits::rearrange_words(6, data, 4);
//     EXPECT_TRUE(res.size() == 5);
//     for (auto i : res) {
//         BL_LOGGER("%d", i);
//     }
// }
