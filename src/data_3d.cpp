//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>
#include <memory>

#include "chunk.h"
#include "utils.h"

namespace bl {
    namespace {
        std::vector<biome> load_subchunk_biome(const byte_t *data, int &read, size_t len) {
            read = 1;

            uint8_t head = data[0];
            if (head == 0xff) return std::vector<biome>(4096, biome::none);

            auto bits = static_cast<uint8_t>(head >> 1);
            int index[4096]{0};
            constexpr auto BLOCK_NUM = 4096;
            int palette_len = 1;
            if (bits != 0) {
                int bpw = 32 / bits;
                auto word_count = BLOCK_NUM / bpw;
                if (BLOCK_NUM % bpw != 0) word_count++;
                int position = 0;

                for (int wordi = 0; wordi < word_count; wordi++) {
                    auto word = *reinterpret_cast<const int *>(data + read + wordi * 4);
                    for (int block = 0; block < bpw; block++) {
                        int state = (word >> ((position % bpw) * bits)) & ((1 << bits) - 1);
                        index[position] = state;
                        position++;
                    }
                }

                read += word_count << 2;
                palette_len = *reinterpret_cast<const int *>(data + read);
                read += 4;
            }

            std::vector<biome> res(4096, bl::biome::none);
            std::vector<biome> biomes_palettes;

            for (int i = 0; i < palette_len; i++) {
                auto biomeId = *reinterpret_cast<const int *>(data + read);
                read += 4;
                biomes_palettes.push_back(static_cast<biome>(biomeId));
            }

            for (int i = 0; i < 4096; i++) {
                if (index[i] >= 0 && index[i] < biomes_palettes.size()) {
                    res[i] = biomes_palettes[index[i]];
                }
            }

            return res;
        }
    }  // namespace
    bool data_3d::load(const byte_t *data, size_t len) {
        int index = 0;
        if (len < 512) {
            BL_ERROR("Invalid Data3d format");
            return false;
        }

        memcpy(this->height_map_.data(), data, 512);
        index += 512;
        while (index < len) {
            int read = 0;

            auto sub_chunk_biome = load_subchunk_biome(data + index, read, len);
            //            BL_LOGGER("size is %d", sub_chunk_biome.size());
            for (int y = 0; y < 16; y++) {
                auto layer = std::array<std::array<biome, 16>, 16>{};
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        layer[x][z] = sub_chunk_biome[x * 256 + z * 16 + y];
                    }
                }
                this->biomes_.push_back(layer);
            }
            index += read;
        }
        if (this->biomes_.size() == 384) {
            this->dim = 0;
        } else if (this->biomes_.size() == 256) {
            this->dim = 2;
        } else if (this->biomes_.size() == 128) {
            this->dim = 1;
        } else {
            BL_ERROR("Invalid Biome layer size");
            return false;
        }
        return true;
    }

    void data_3d::dump_to_file(FILE *fp) const {
        for (int i = 0; i < 256; i++) {
            fprintf(fp, "%03d ", this->height_map_[i] - 64);
            if (i % 16 == 15) {
                fprintf(fp, "\n");
            }
        }
    }
    biome data_3d::get_biome(int cx, int y, int cz) {
        if (dim == 0) y += 64;
        if (y > this->biomes_.size()) {
            return biome::none;
        }
        return this->biomes_[y][cx][cz];
    }

    std::array<std::array<biome, 16>, 16> data_3d::get_biome_y(int y) {
        if (dim == 0) y += 64;
        if (y > this->biomes_.size()) {
            return {};
        }
        return this->biomes_[y];
    }

    biome data_3d::get_top_biome(int cx, int cz) {
        int y = (int)this->biomes_.size() - 1;
        while (y >= 0 && this->biomes_[y][cx][cz] == none) {
            y--;
        }
        return y < 0 ? biome::none : this->biomes_[y][cx][cz];
    }
}  // namespace bl
