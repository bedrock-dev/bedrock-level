//
// Created by xhy on 2023/3/29.
//

#include "bit_tools.h"

namespace bl::bits {


    namespace {
        std::vector<int16_t> word_1(const uint8_t *data, size_t len) {

            std::vector<int16_t> res;
            res.reserve(len << 3);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                res.push_back(static_cast<int16_t>(v & 0b10000000));
                res.push_back(static_cast<int16_t>(v & 0b01000000));
                res.push_back(static_cast<int16_t>(v & 0b00100000));
                res.push_back(static_cast<int16_t>(v & 0b00010000));
                res.push_back(static_cast<int16_t>(v & 0b00001000));
                res.push_back(static_cast<int16_t>(v & 0b00000100));
                res.push_back(static_cast<int16_t>(v & 0b00000010));
                res.push_back(static_cast<int16_t>(v & 0b00000001));
            }
            return res;
        }

        std::vector<int16_t> word_2(const uint8_t *data, size_t len) {
            std::vector<int16_t> res;
            res.reserve(len << 2);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                res.push_back(static_cast<int16_t>(v & 0b11000000));
                res.push_back(static_cast<int16_t>(v & 0b00110000));
                res.push_back(static_cast<int16_t>(v & 0b00001100));
                res.push_back(static_cast<int16_t>(v & 0b00000011));
            }
            return res;
        }

        std::vector<int16_t> word_4(const uint8_t *data, size_t len) {
            std::vector<int16_t> res;
            res.reserve(len << 1);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                res.push_back(static_cast<int16_t>(v & 0b11110000));
                res.push_back(static_cast<int16_t>(v & 0b00001111));
            }
            return res;
        }

        std::vector<int16_t> word_8(const uint8_t *data, size_t len) {
            std::vector<int16_t> res;
            res.reserve(len);
            for (auto i = 0; i < len; i++) {
                res.push_back(static_cast<int16_t>(data[i]));
            }
            return res;
        }

        std::vector<int16_t> word_16(const uint8_t *data, size_t len) {
            auto sz = len >> 1;
            std::vector<int16_t> res;
            res.reserve(sz);

            for (auto i = 0; i < sz; i++) {
                auto v1 = static_cast<int16_t>(data[sz << 1]);
                auto v2 = static_cast<int16_t>(data[(sz << 1) + 1]);
                res.push_back(static_cast<int16_t>(v1 << 8u) | v2);
            }
            return res;
        }


    }


    std::vector<int16_t> rearrange_words(size_t bits_len, const uint8_t *data, size_t len) {
        Assert(len % 4 == 0, "Invalid World input.");
        if (bits_len == 1) {
        }
    }




}


