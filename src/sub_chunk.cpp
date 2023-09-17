//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <cstdio>

#include "bit_tools.h"
#include "utils.h"

// #include "nbt.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "color.h"
#include "palette.h"

namespace bl {

    namespace {

        // sub chunk layout
        // https://user-images.githubusercontent.com/13713600/148380033-6223ac76-54b7-472c-a355-5923b87cb7c5.png

        bool read_header(sub_chunk *sub_chunk, const byte_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            // assert that stream is long enough
            auto version = stream[0];
            if (version != 8        // 1.2~1.17
                && version != 9) {  // 1.18+
                BL_LOGGER("Unsupported sub chunk version: %u", stream[0]);
                return false;
            }
            sub_chunk->set_version(version);
            sub_chunk->set_layers_num(stream[1]);
            read = 2;
            // y-index for version 9
            if (version == 9) {
                int8_t y_index = stream[2];
                if (y_index != sub_chunk->y_index()) {
                    BL_ERROR("Invalid Y index value(new(%d)  != default(%d))", y_index,
                             sub_chunk->y_index());
                }
                sub_chunk->set_y_index(y_index);
                read++;
            }
            return true;
        }

        bool read_palettes(bl::sub_chunk::layer *layer, const byte_t *stream, size_t number,
                           size_t len, int &read) {
            read = 0;
            for (auto i = 0u; i < number; i++) {
                int r = 0;
                auto *tag = bl::palette::read_one_palette(stream + read, r);
                if (tag) {
                    tag->remove("version");  // remove version tag(compatibility for color table)
                    layer->palettes.push_back(tag);
                } else {
                    BL_ERROR("Can not read block palette");
                    return false;
                }
                read += r;
            }
            return true;
        }

        bool read_one_layer(bl::sub_chunk::layer *layer, const byte_t *stream, size_t len,
                            int &read) {
            read = 0;
            constexpr auto BLOCK_NUM = 16 * 16 * 16;
            if (!layer || !stream) return false;
            auto layer_header = stream[0];
            read++;
            layer->type = layer_header & 0x1;
            layer->bits = layer_header >> 1u;
            if (layer->bits != 0) {
                int block_per_word = 32 / layer->bits;
                auto wordCount = BLOCK_NUM / block_per_word;
                if (BLOCK_NUM % block_per_word != 0) wordCount++;
                layer->blocks.resize(BLOCK_NUM);
                int position = 0;
                for (int wordi = 0; wordi < wordCount; wordi++) {
                    auto word = *reinterpret_cast<const int *>(stream + read + wordi * 4);
                    for (int block = 0; block < block_per_word; block++) {
                        int state = (word >> ((position % block_per_word) * layer->bits)) &
                                    ((1 << layer->bits) - 1);
                        if (position < static_cast<int>(layer->blocks.size())) {
                            layer->blocks[position] = state;
                        }
                        position++;
                    }
                }

                read += wordCount << 2;
                int palette_len = *reinterpret_cast<const int *>(stream + read);
                layer->palette_len = palette_len;
                read += 4;
            } else {  // uniform
                layer->blocks = std::vector<uint16_t>(4096, 0);
                layer->palette_len = 1;
            }
            // palette header
            int palette_read = 0;
            read_palettes(layer, stream + read, layer->palette_len, len - read, palette_read);
            read += palette_read;
            return true;
        }
    }  // namespace

    bool sub_chunk::load(const byte_t *data, size_t len) {
        size_t idx = 0;  // 全局索引
        int read{0};
        if (!read_header(this, data, read)) return false;
        idx += read;
        for (auto i = 0; i < (int)this->layers_num_; i++) {
            this->layers_.push_back(new layer());
            if (!read_one_layer(this->layers_.back(), data + idx, len - idx, read)) {
                BL_ERROR("can not read layer %d", i);
                return false;
            }
            idx += read;
        }
        return true;
    }

    void sub_chunk::dump_to_file(FILE *fp) const {}

    block_info sub_chunk::get_block(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            BL_ERROR("Invalid in chunk position %d %d %d", rx, ry, rz);
            return {};
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        if (block >= this->layers_[0]->palettes.size() || block < 0) {
            BL_ERROR("Invalid block index with value %d", block);
            return {};
        }

        auto &palette = this->layers_[0]->palettes[block];

        auto id = palette->value.find("name");
        if (id == palette->value.end()) {
            return {};
        }

        return {dynamic_cast<bl::palette::string_tag *>(id->second)->value,
                bl::get_block_color_from_SNBT(palette->to_raw())};
    }

    block_info sub_chunk::get_block_fast(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            BL_ERROR("Invalid in chunk position %d %d %d", rx, ry, rz);
            return {};
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        if (block >= this->layers_[0]->palettes.size() || block < 0) {
            BL_ERROR("Invalid block index with value %d", block);
            return {};
        }

        auto &palette = this->layers_[0]->palettes[block];
        auto id = palette->value.find("name");
        if (id == palette->value.end()) {
            return {};
        }

        return {dynamic_cast<bl::palette::string_tag *>(id->second)->value, bl::color{}};
    }

    palette::compound_tag *sub_chunk::get_block_raw(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            BL_ERROR("Invalid in chunk position %d %d %d", rx, ry, rz);
            return nullptr;
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        if (block >= this->layers_[0]->palettes.size() || block < 0) {
            BL_ERROR("Invalid block index with value %d", block);
            return nullptr;
        }

        return this->layers_[0]->palettes[block];
    }
    sub_chunk::~sub_chunk() {
        for (auto &layer : this->layers_) {
            delete layer;
        }
    }

    sub_chunk::layer::~layer() {
        for (auto &p : this->palettes) delete p;
    }
}  // namespace bl
