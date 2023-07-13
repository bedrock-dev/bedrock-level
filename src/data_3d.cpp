//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>
#include <memory>

#include "bedrock_key.h"
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
    bool biome3d::load_from_d3d(const byte_t *data, size_t len) {
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
        return true;
    }

    biome biome3d::get_biome(int cx, int y, int cz) {
        if (this->version_ == Old) {
            return this->biomes_.empty() ? bl::biome::none : this->biomes_[0][cx][cz];
        }
        auto [my, _] = pos_.get_y_range(this->version_);
        y += my;
        if (y > this->biomes_.size() || y < 0) {
            return biome::none;
        }
        return this->biomes_[y][cx][cz];
    }

    std::array<std::array<biome, 16>, 16> biome3d::get_biome_y(int y) {
        if (this->version_ == Old) {
            return this->biomes_.empty() ? std::array<std::array<biome, 16>, 16>()
                                         : this->biomes_[0];
        }
        auto [my, _] = pos_.get_y_range(this->version_);
        y += my;
        if (y > this->biomes_.size()) {
            return {};
        }
        return this->biomes_[y];
    }

    biome biome3d::get_top_biome(int cx, int cz) {
        if (this->version_ == Old) return this->get_biome(cx, 0, cz);
        int y = (int)this->biomes_.size() - 1;
        while (y >= 0 && this->biomes_[y][cx][cz] == none) {
            y--;
        }
        return y < 0 ? biome::none : this->biomes_[y][cx][cz];
    }
    bool biome3d::load_from_d2d(const byte_t *data, size_t len) {
        if (len != 768) {  // height map: 512bytes biome: 256 * 4 = 1024 bytes
            BL_ERROR("Invalid Data2d format (%zu)", len);
            return false;
        }
        memcpy(this->height_map_.data(), data, 512);
        auto layer = std::array<std::array<biome, 16>, 16>{};
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                layer[x][z] = static_cast<biome>(data[512 + x + 16 * z]);
            }
        }
        this->biomes_.push_back(layer);
        return true;
    }

}  // namespace bl
