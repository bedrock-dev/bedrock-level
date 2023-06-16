//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_BIT_TOOLS_H
#define BEDROCK_LEVEL_BIT_TOOLS_H

#include <cassert>
#include <cstdint>
#include <vector>

#include "utils.h"

namespace bl::bits {

    inline uint8_t mask(uint8_t low, uint8_t high) { return (0xff >> (7u - high) & (0xff << low)); }

    //
    //    template<typename T>
    //    std::vector<T> rearrange_bytes(size_t bit_len, const uint8_t *data, size_t len) {
    //        Assert(bit_len > 0 || bit_len < 8, "Bit len should within [1,7]");
    //        Assert((len << 3) % bit_len == 0, "Invalid len of data");
    //        Assert(bit_len <= sizeof(T) << 3, "Container is too small");
    //        std::vector<T> res;
    //
    //        size_t index{0};
    //        uint8_t offset{0};
    //        const uint8_t R_OFF = 8 - bit_len;
    //        while (true) {
    //            // 开读
    //            uint8_t v = data[index];
    //            auto next = bit_len + offset;
    //            if (next < 8) {  // 内部偏移即可
    //                res.push_back((uint8_t) (v << offset) >> R_OFF);
    //                offset = next;
    //            } else if (next == 8) {
    //                res.push_back((uint8_t) (v << offset) >> R_OFF);
    //                offset = 0;
    //                index++;
    //                if (index >= len) break;
    //            } else {
    //                auto remain = next - 8;
    //                res.push_back(((mask(0, 7u - offset) & v) << remain) +
    //                              (data[index + 1] >> (8 - remain)));
    //                offset = remain;
    //                index++;
    //            }
    //        }
    //        return res;
    //    }
    //
    //    //https://gist.github.com/Tomcc/a96af509e275b1af483b25c543cfbf37
    //    /**
    //     * total 4096 block
    //     * enum class Type : uint8_t {
    //	Paletted1 = 1,   // 32 blocks per word          --> 128 world (512bytes)
    //	Paletted2 = 2,   // 16 blocks per word          --> 256 world (1024bytes)
    //	Paletted3 = 3,   // 10 blocks and 2 bits of padding per word    409 word + 6block(?3bytes?)
    //	Paletted4 = 4,   // 8 blocks per word           --> 512 world (2048bytes)
    //	Paletted5 = 5,   // 6 blocks and 2 bits of padding per word     682word + 4block(?3bytes?)
    //	Paletted6 = 6,   // 5 blocks and 2 bits of padding per word     819word + 1block(?1bytes?)
    //	Paletted8  = 8,  // 4 blocks per word           --> 1024 world (4096bytes)
    //	Paletted16 = 16, // 2 blocks per word           --> 2048 word (8192bytes)
    //    }
    //     */
    //

    std::vector<uint16_t> rearrange_words(size_t bits_len, const byte_t *data, size_t len);

}  // namespace bl::bits

#endif  // BEDROCK_LEVEL_BIT_TOOLS_H
