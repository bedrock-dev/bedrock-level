//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <array>
#include <cstdint>
#include <vector>
#include <cstdio>

namespace bl {
    class block_palette {
        block_palette() = default;

    private:
        uint8_t bits_;
    };


    class sub_chunk {
    public:


        struct layer {
            std::array<int, 16 * 16 * 16> blocks_;
            block_palette palette_;
        };

        sub_chunk() = default;

        void set_version(uint8_t version) { this->version_ = version; }

        void set_y_index(uint8_t y_index) { this->y_index_ = y_index; }

        void set_layers_num(uint8_t layers_num) { this->layers_num_ = layers_num; }

        bool load(const uint8_t *data, size_t len);


        //for develop

        void dump_to_file(FILE *fp) const;

    private:

        uint8_t version_{0xff};
        uint8_t y_index_{0xff};
        uint8_t layers_num_{0xff};

        std::vector<layer> layers_;

    };
}


#endif //BEDROCK_LEVEL_SUB_CHUNK_H
