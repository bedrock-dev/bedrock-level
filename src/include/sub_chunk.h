//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "palette.h"

namespace bl {
    struct block_info {
        std::string name{"minecraft:unknown"};
    };

    class sub_chunk {
       public:
        struct layer {
            layer() = default;
            uint8_t bits{};
            uint8_t type{};
            uint32_t palette_len{};
            std::vector<uint16_t> blocks{};
            std::vector<bl::palette::compound_tag *> palettes;

            ~layer();
        };

        block_info get_block(int rx, int ry, int rz);

        palette::compound_tag *get_block_raw(int rx, int ry, int rz);

        sub_chunk() = default;

        void set_version(uint8_t version) { this->version_ = version; }

        void set_y_index(int y_index) { this->y_index_ = y_index; }

        void set_layers_num(uint8_t layers_num) { this->layers_num_ = layers_num; }

        bool load(const byte_t *data, size_t len);

        // for develop
        void dump_to_file(FILE *fp) const;

        ~sub_chunk();

       private:
        void push_back_layer(layer *layer) { this->layers_.push_back(layer); }

        uint8_t version_{0xff};
        int y_index_{0};
        uint8_t layers_num_{0xff};
        std::vector<layer *> layers_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_SUB_CHUNK_H
