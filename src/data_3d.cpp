//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>
#include <memory>

#include "chunk.h"
#include "utils.h"

namespace bl {

    std::array<biome, 4096> load_subchunk_biome(const byte_t *data, int &read) {
        read = 1;
        std::array<biome, 4096> res{};
        res.fill(biome::none);

        uint8_t head = data[0];
        if (head == 0xff) {
            return res;
        }
        auto bits = static_cast<uint8_t>(head >> 1);
        std::vector<int> index(4096, 0);
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
        std::vector<biome> biomes_palettes;
        for (int i = 0; i < palette_len; i++) {
            auto biomeId = *reinterpret_cast<const int *>(data + read);
            read += 4;
            biomes_palettes.push_back(static_cast<biome>(biomeId));
        }

        for (int i = 0; i < 4096; i++) {
            res[i] = biomes_palettes[index[i]];
        }

        return res;
    }

    bool data_3d::load(const byte_t *data, size_t len) {
        int index = 0;
        if (len < 512) {
            BL_LOGGER("Invalid Data3d format");
            return false;
        }
        memcpy(this->height_map_.data(), data, 512);
        index += 512;
        while (index < len) {
            int read = 0;
            this->biomes_.push_back(load_subchunk_biome(data + index, read));
            index += read;
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
        int index;
        int cy;
        chunk::map_y_to_subchunk(y, index, cy);
        if (index > this->biomes_.size()) {
            return none;
        }
        
        return this->biomes_[index][cy + cz * 16 + cx * 256];
    }

}  // namespace bl
