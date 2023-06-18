//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>
#include <memory>

#include "utils.h"

namespace bl {

    void load_subchunk_biome(const byte_t *data, int &read) {
        read = 1;
        uint8_t head = data[0];
        if (head == 0xff) {
            return;
        }
        int type = head & 1;
        auto bits = static_cast<uint8_t>(head >> 1);

        BL_LOGGER("Bits = %u type = %d", bits, type);
        constexpr auto BLOCK_NUM = 4096;
        int palette_len = 1;
        if (bits != 0) {
            int block_per_word = 32 / bits;
            auto wordCount = BLOCK_NUM / block_per_word;
            if (BLOCK_NUM % block_per_word != 0) wordCount++;
            int position = 0;
            for (int wordi = 0; wordi < wordCount; wordi++) {
                auto word = *reinterpret_cast<const int *>(data + read + wordi * 4);
                for (int block = 0; block < block_per_word; block++) {
                    int state = (word >> ((position % block_per_word) * bits)) & ((1 << bits) - 1);
                    //                    printf("state  = %d\n", state);
                    position++;
                }
            }

            read += wordCount << 2;
            palette_len = *reinterpret_cast<const int *>(data + read);
            read += 4;
        }
        std::vector<int> biomes_palettes;
        for (int i = 0; i < palette_len; i++) {
            auto biomeId = *reinterpret_cast<const int *>(data + read);
            read += 4;
            biomes_palettes.push_back(biomeId);
        }
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
            load_subchunk_biome(data + index, read);
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

}  // namespace bl
