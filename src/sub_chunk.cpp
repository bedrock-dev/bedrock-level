//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <cstdio>

#include "bit_tools.h"
#include "utils.h"

// #include "nbt.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

#include "palette.h"

namespace bl {

    namespace {

        void read_block_data(std::array<int16_t, 4096> &block_data, int bits, const byte_t *data,
                             size_t len) {
            int index = 0;
            while (index < 4096) {
            }
        }

        // sub chunk layout
        // https://user-images.githubusercontent.com/13713600/148380033-6223ac76-54b7-472c-a355-5923b87cb7c5.png

        bool read_header(sub_chunk *sub_chunk, const byte_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            // assert that stream is long enough
            sub_chunk->set_version(stream[0]);
            sub_chunk->set_layers_num(stream[1]);
            sub_chunk->set_y_index(static_cast<int8_t>(stream[2]));
            read = 3;
            return true;
        }

        bool read_palettes(bl::sub_chunk::layer *layer, const byte_t *stream, size_t number,
                           size_t len, int &read) {
            read = 0;
            for (int i = 0; i < number; i++) {
                int r = 0;
                auto *tag = bl::palette::read_one_palette(stream + read, r);
                if (tag) {
                    layer->palettes.push_back(tag);
                } else {
                    throw std::runtime_error("Error read palette");
                }
                read += r;
            }

            return true;
        }

        bool read_one_layer(sub_chunk *sub_chunk, const byte_t *stream, size_t len, int &read) {
            read = 0;
            constexpr auto BLOCK_NUM = 16 * 16 * 16;
            if (!sub_chunk || !stream) return false;
            bl::sub_chunk::layer layer;
            auto layer_header = stream[0];
            read++;
            layer.type = layer_header & 0x1;
            layer.bits = layer_header >> 1u;

            if (layer.bits != 0) {
                int block_per_word = 32 / layer.bits;
                auto words = BLOCK_NUM / block_per_word;
                if (BLOCK_NUM % block_per_word != 0) words++;
                layer.blocks = bits::rearrange_words(layer.bits, stream + read, words << 2);
                Assert(layer.blocks.size() >= BLOCK_NUM, "Invalid block data len");
                layer.blocks.resize(BLOCK_NUM);
                //   BL_LOGGER("blocks number:%d , bit len = %d", layer.blocks.size(), layer.bits);
                read += words << 2;
                layer.palette_len = *reinterpret_cast<const uint32_t *>(stream + read);
                read += 4;
            } else {
                // uniform
                layer.blocks = std::vector<uint16_t>(4096, 0);
                layer.palette_len = 1;
            }
            // palette header
            int palette_read = 0;
            read_palettes(&layer, stream + read, layer.palette_len, len - read, palette_read);
            read += palette_read;
            sub_chunk->push_back_layer(layer);
            return true;
        }
    }  // namespace

    bool sub_chunk::load(const byte_t *data, size_t len) {
        size_t idx = 0;  // 全局索引
        int read{0};
        if (!read_header(this, data, read)) return false;
        idx += read;
        for (auto i = 0; i < (int)this->layers_num_; i++) {
            if (!read_one_layer(this, data + idx, len - idx, read)) {
                BL_ERROR("can not read layer %d", i);
                return false;
            }
            idx += read;
        }
        return true;
    }

    void sub_chunk::dump_to_file(FILE *fp) const {
        fprintf(fp, "Version : %u\n", this->version_);
        fprintf(fp, "Y index : %d\n", this->y_index_);
        fprintf(fp, "Layers  : %u\n", this->layers_num_);
        fprintf(fp, "===========================================\n");
        size_t index = 0;
        for (auto &layer : this->layers_) {
            fprintf(fp, "Layer %zu:\n", index);
            fprintf(fp, "Bits per block: %d\n", layer.bits);
            fprintf(fp, "Palette type: %d\n", layer.type);
            fprintf(fp, "Palette len: %d\n", layer.palette_len);
            auto i = 0;
            for (auto b : layer.blocks) {
                printf("%02d ", b);
                i++;
                if (i % 16 == 0) {
                    printf("\n");
                }
                if (i % 256 == 0) {
                    printf("------------------------------------------\n");
                }
            }
            for (auto palette : layer.palettes) {
                palette->write(std::cout, 0);
            }

            ++index;
        }
    }

    block_info sub_chunk::get_block(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            BL_ERROR("Invalid in chunk position %d %d %d", rx, ry, rz);
            return {};
        }
        BL_LOGGER("Read in sub chunk position %d %d %d", rx, ry, rz);
        auto idx = ry * 256 + rz * 16 + rx;
        auto block = this->layers_[0].blocks[idx];

        if (block >= this->layers_[0].palettes.size()) {
            BL_ERROR("Invalid block index with value %d", block);
            return {};
        }

        auto *palette = this->layers_[0].palettes[block];
        auto id = palette->value.find("name");
        if (id == palette->value.end()) {
            return {};
        }
        return {dynamic_cast<bl::palette::string_tag *>(id->second)->value};
    }

}  // namespace bl
