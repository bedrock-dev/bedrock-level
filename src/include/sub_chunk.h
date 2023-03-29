//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <array>
#include <cstdint>
#include <vector>

namespace bl {
    class block_palette {
    private:
        uint8_t bits_;
    };


    class sub_chunk {

        struct layer {
            std::array<int, 16 * 16 * 16> blocks_;
            block_palette palette_;
        };


        sub_chunk() = default;

        bool load(const char *data, size_t len) {
            //todo
            return true;
        }


    private:
        uint8_t version_{0xff};
        uint8_t y_index_{0xff};
        std::vector<layer> layers_;

    };
}


#endif //BEDROCK_LEVEL_SUB_CHUNK_H
