//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_BIT_TOOLS_H
#define BEDROCK_LEVEL_BIT_TOOLS_H

#include <vector>
#include <cstdint>
#include "utils.h"
#include <cassert>

namespace bl::bits {


    inline uint8_t mask(uint8_t low, uint8_t high) {
        return (0xff >> (7u - high) & (0xff << low));
    }


//    template<typename T>
//    std::vector<T> combineBytes(size_t bit_len, const uint8_t *data, size_t len) {
//        //TODO
//        if (bit_len <= 0 || bit_len % 8 != 0) {
//            return {};
//        }
//        return {};
//    }

    template<typename T>
    std::vector<T> restructureBytes(size_t bit_len, const uint8_t *data, size_t len) {
        Assert(bit_len > 0 || bit_len < 8, "Bit len should within [1,7]");
        Assert((len << 3) % bit_len == 0, "Invalid len of data");
        Assert(bit_len <= sizeof(T) << 3, "Container is too small");
        std::vector<T> res;

        size_t index{0};
        uint8_t offset{0};
        const uint8_t R_OFF = 8 - bit_len;
        while (true) {
            //开读
            uint8_t v = data[index];
            auto next = bit_len + offset;
            if (next < 8) { //内部偏移即可
                res.push_back((uint8_t) (v << offset) >> R_OFF);
                offset = next;
            } else if (next == 8) {
                res.push_back((uint8_t) (v << offset) >> R_OFF);
                offset = 0;
                index++;
                if (index >= len)break;
            } else {
                auto remain = next - 8;
                res.push_back(((mask(0, 7u - offset) & v) << remain) + (data[index + 1] >> (8 - remain)));
                offset = remain;
                index++;
            }
        }
        return res;
    }
}

#endif //BEDROCK_LEVEL_BIT_TOOLS_H
