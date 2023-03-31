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

        // sub chunk layout
        // https://user-images.githubusercontent.com/13713600/148380033-6223ac76-54b7-472c-a355-5923b87cb7c5.png

        bool read_header(sub_chunk *sub_chunk, const uint8_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            // assert that stream is long enough
            sub_chunk->set_version(stream[0]);
            sub_chunk->set_layers_num(stream[1]);
            sub_chunk->set_y_index(stream[2]);
            read = 3;
            return true;
        }

        bool read_palettes(bl::sub_chunk::layer *layer, const uint8_t *stream, size_t number,
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

        bool read_one_layer(sub_chunk *sub_chunk, const uint8_t *stream, size_t len, int &read) {
            constexpr auto BLOCK_NUM = 16 * 16 * 16;
            if (!sub_chunk || !stream) return false;
            read = 0;
            bl::sub_chunk::layer layer;
            auto layer_header = stream[0];
            read++;
            layer.type = layer_header & 0x1;
            layer.bits = layer_header >> 1u;
            BL_LOGGER("layer bits is %u\n", layer.bits);
            const auto read_bytes = (layer.bits * BLOCK_NUM) >> 3;
            if (layer.bits < 8) {
                layer.blocks =
                        bl::bits::restructureBytes<int>(layer.bits, stream + read, read_bytes);
            } else {
                BL_ERROR("Layer bits is %d\n", layer.bits);
            }
            read += read_bytes;

            // palette header
            layer.palette_len = *reinterpret_cast<const uint32_t *>(stream + read);
            read += 4;
            int palette_read = 0;
            read_palettes(&layer, stream + read, layer.palette_len, len - read, palette_read);
            read += palette_read;
            sub_chunk->push_back_layer(layer);
            return true;
        }
    }  // namespace

    bool sub_chunk::load(const uint8_t *data, size_t len) {
        size_t idx = 0;  // 全局索引
        int read{0};
        if (!read_header(this, data, read)) {
            return false;
        }
        idx += read;
        for (int i = 0; i < this->layers_num_; i++) {
            BL_LOGGER("Read layer: %d", i);
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
        fprintf(fp, "Y index : %u\n", this->y_index_);
        fprintf(fp, "Layers  : %u\n", this->layers_num_);
        fprintf(fp, "===========================================\n");
        size_t index = 0;
        for (auto &layer: this->layers_) {
            fprintf(fp, "Layer %zu:\n", index);
            fprintf(fp, "Bits per block: %d\n", layer.bits);
            fprintf(fp, "Palette type: %d\n", layer.type);
            fprintf(fp, "Palette len: %d\n", layer.palette_len);
            auto i = 0;
            for (auto b: layer.blocks) {
                printf("%02d ", b);
                i++;
                if (i % 16 == 0) {
                    printf("\n");
                }
                if (i % 256 == 0) {
                    printf("------------------------------------------\n");
                }
            }
            for (auto palette: layer.palettes) {
                palette->write(std::cout, 0);
            }

            ++index;
        }
    }
}  // namespace bl
