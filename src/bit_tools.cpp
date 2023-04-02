//
// Created by xhy on 2023/3/29.
//

#include "bit_tools.h"

namespace bl::bits {


    namespace {


        std::vector<uint16_t> word_1(const byte_t *data, size_t len) {
            static uint8_t M[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

            std::vector<uint16_t> res(len << 3, 0);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                for (int j = 0; j < 8; j++) {
                    res[(i << 3) + j] = (v & M[j]) >> (7 - j);
                }

            }
            return res;
        }

        std::vector<uint16_t> word_2(const byte_t *data, size_t len) {
            std::vector<uint16_t> res(len << 2, 0);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                res[(i << 2) + 0] = ((int16_t) (v & 0b11000000)) >> 6;
                res[(i << 2) + 1] = ((int16_t) (v & 0b00110000)) >> 4;
                res[(i << 2) + 2] = ((int16_t) (v & 0b00001100)) >> 2;
                res[(i << 2) + 3] = ((int16_t) (v & 0b00000011));
            }
            return res;
        }

        std::vector<uint16_t> word_4(const byte_t *data, size_t len) {
            std::vector<uint16_t> res(len << 1, 0);
            for (auto i = 0; i < len; i++) {
                auto v = data[i];
                res[(i << 1) + 0] = ((int16_t) (v & 0b11110000)) >> 4;
                res[(i << 1) + 1] = ((int16_t) (v & 0b00001111));
            }
            return res;
        }

        std::vector<uint16_t> word_8(const byte_t *data, size_t len) {
            std::vector<uint16_t> res(len, 0);
            for (auto i = 0; i < len; i++) {
                res.push_back((int16_t) data[i]);
            }
            return res;
        }

        std::vector<uint16_t> word_16(const byte_t *data, size_t len) {
            auto sz = len >> 1;
            std::vector<uint16_t> res;
            res.reserve(sz);

            for (auto i = 0; i < sz; i++) {
                auto v1 = (uint16_t) static_cast<unsigned char >(data[i << 1]);
                auto v2 = (uint16_t) static_cast<unsigned char >(data[(i << 1) + 1]);
                res.push_back((uint16_t) ((v1 << 8u) | v2));
            }

            return res;
        }


        void split_word(size_t bit_len, const byte_t *data, uint16_t *res) {

            size_t index{0};
            uint8_t offset{0};
            size_t res_idx = 0;
            const uint8_t R_OFF = 8 - bit_len;
            const auto len = 32 / bit_len;
            while (res_idx < len) {// 开读
                auto v = static_cast<uint8_t>(data[index]);
                auto next = bit_len + offset;
                if (next < 8) {  // 内部偏移即可
                    res[res_idx] = ((uint8_t) (v << offset) >> R_OFF);
                    offset = next;
                } else if (next == 8) {
                    res[res_idx] = ((uint8_t) (v << offset) >> R_OFF);
                    offset = 0;
                    index++;
                } else {
                    auto remain = next - 8;
                    res[res_idx] = (((mask(0, 7u - offset) & v) << remain) +
                                    (static_cast<uint8_t>(data[index + 1]) >> (8 - remain)));
                    offset = remain;
                    index++;
                }
                res_idx++;
            }
        }

        std::vector<uint16_t> word_n(const byte_t *data, size_t n, size_t bit_len) {
            auto word_num = n / 4;
            auto npw = 32 / bit_len;
            std::vector<uint16_t> res(npw * word_num, 0);
            size_t res_idx{0};
            for (int i = 0; i < word_num; i++) {
                split_word(bit_len, data + i, res.data() + res_idx);
                res_idx += npw;
            }
            return res;
        }
    }

    std::vector<uint16_t> rearrange_words(size_t bits_len, const byte_t *data, size_t len) {
        Assert(len % 4 == 0, "Invalid World input.");
        switch (bits_len) {
            case 1:
                return word_1(data, len);
            case 2:
                return word_2(data, len);
            case 4:
                return word_4(data, len);
            case 8:
                return word_8(data, len);
            case 16:
                return word_16(data, len);
            default:
                return word_n(data, len, bits_len);
        }
    }

}


