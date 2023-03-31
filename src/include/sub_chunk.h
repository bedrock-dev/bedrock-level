//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "palette.h"

namespace bl {

    class sub_chunk {
       public:
        struct layer {
            uint8_t bits;
            uint8_t type;
            uint32_t palette_len;
            std::vector<int> blocks;
            std::vector<palette::compound_tag *> palettes;
        };

        sub_chunk() = default;

        void set_version(uint8_t version) { this->version_ = version; }

        void set_y_index(uint8_t y_index) { this->y_index_ = y_index; }

        void set_layers_num(uint8_t layers_num) { this->layers_num_ = layers_num; }

        bool load(const uint8_t *data, size_t len);

        // for develop

        void dump_to_file(FILE *fp) const;

        void push_back_layer(const layer &layer) { this->layers_.push_back(layer); }

       private:
        uint8_t version_{0xff};
        uint8_t y_index_{0xff};
        uint8_t layers_num_{0xff};

        std::vector<layer> layers_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_SUB_CHUNK_H
