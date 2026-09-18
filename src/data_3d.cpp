//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>

#include "bedrock_key.h"
#include "utils.h"

namespace bl {
    namespace {
        std::vector<biome> load_subchunk_biome(const byte_t* data, int& read, size_t len) {
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
                    auto word = *reinterpret_cast<const int*>(data + read + wordi * 4);
                    // word_count * bpw can exceed 4096 when bits does not divide 32
                    // (e.g. bits 3/5/6); stop decoding once all entries are filled.
                    for (int block = 0; block < bpw && position < BLOCK_NUM; block++) {
                        int state = (word >> ((position % bpw) * bits)) & ((1 << bits) - 1);
                        index[position] = state;
                        position++;
                    }
                }

                read += word_count << 2;
                palette_len = *reinterpret_cast<const int*>(data + read);
                read += 4;
            }

            std::vector<biome> res(4096, bl::biome::none);
            std::vector<biome> biomes_palettes;

            for (int i = 0; i < palette_len; i++) {
                auto biomeId = *reinterpret_cast<const int*>(data + read);
                read += 4;
                biomes_palettes.push_back(static_cast<biome>(biomeId));
            }

            for (int i = 0; i < 4096; i++) {
                if (index[i] >= 0 && index[i] < static_cast<int>(biomes_palettes.size())) {
                    res[i] = biomes_palettes[index[i]];
                }
            }

            return res;
        }

        void append_i32(std::string& out, int32_t v) {
            out.push_back(static_cast<char>(v & 0xff));
            out.push_back(static_cast<char>((v >> 8) & 0xff));
            out.push_back(static_cast<char>((v >> 16) & 0xff));
            out.push_back(static_cast<char>((v >> 24) & 0xff));
        }

        // Reverse of load_subchunk_biome(): packs one biome sub-chunk. values is indexed
        // x * 256 + z * 16 + y, the order the on-disk array uses.
        void append_packed_biome_sub_chunk(std::string& out, const std::vector<biome>& values) {
            std::array<int32_t, 256> slot;
            slot.fill(-1);
            std::vector<biome> palette;
            std::vector<int32_t> index(values.size(), 0);
            for (size_t i = 0; i < values.size(); i++) {
                const auto id = static_cast<uint8_t>(values[i]);
                if (slot[id] < 0) {
                    slot[id] = static_cast<int32_t>(palette.size());
                    palette.push_back(values[i]);
                }
                index[i] = slot[id];
            }

            if (palette.size() <= 1) {
                // 0xff is what the reader (and the game) uses for a sub-chunk that holds no
                // biome record at all; a single biome needs no index array either.
                if (palette.empty() || palette[0] == biome::none) {
                    out.push_back(static_cast<char>(0xff));
                    return;
                }
                out.push_back('\0');
                append_i32(out, static_cast<int32_t>(palette[0]));
                return;
            }

            int bits = 1;
            while ((1 << bits) < static_cast<int>(palette.size())) bits++;
            out.push_back(static_cast<char>(bits << 1));

            const int bpw = 32 / bits;
            const int total = static_cast<int>(index.size());
            for (int word = 0; word * bpw < total; word++) {
                uint32_t packed = 0;
                for (int s = 0; s < bpw; s++) {
                    const int pos = word * bpw + s;
                    if (pos >= total) break;
                    packed |= static_cast<uint32_t>(index[pos]) << (s * bits);
                }
                append_i32(out, static_cast<int32_t>(packed));
            }
            append_i32(out, static_cast<int32_t>(palette.size()));
            for (const auto b : palette) append_i32(out, static_cast<int32_t>(b));
        }
    }  // namespace
    bool biome3d::load_from_d3d(const byte_t* data, size_t len) {
        int index = 0;
        if (len < 512) {
            LOG_F(ERROR, "Invalid Data3d format");
            return false;
        }
        this->use_3d_biome_maps_ = true;
        memcpy(this->height_map_.data(), data, 512);
        index += 512;
        while (index < static_cast<int>(len)) {
            int read = 0;
            auto sub_chunk_biome = load_subchunk_biome(data + index, read, len);
            for (int y = 0; y < 16; y++) {
                std::array<biome, 256> layer{};
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        layer[x * 16 + z] = sub_chunk_biome[x * 256 + z * 16 + y];
                    }
                }
                this->biomes_.push_back(layer);
            }
            index += read;
        }
        return true;
    }

    biome biome3d::get_biome(int cx, int y, int cz) {
        if (!this->use_3d_biome_maps_) {
            return this->biomes_.empty() ? bl::biome::none : this->biomes_[0][cx * 16 + cz];
        }
        y -= dimension_min_y(this->pos_.dim);

        //        printf("y = %d\n", y);
        if (y >= static_cast<int>(this->biomes_.size()) || y < 0) {
            return biome::none;
        }
        return this->biomes_[y][cx * 16 + cz];
    }

    std::vector<std::vector<biome>> biome3d::get_biome_y(int y) {
        std::vector<std::vector<biome>> layer(16, std::vector<biome>(16, bl::biome::none));
        if (!this->use_3d_biome_maps_) {
            if (!this->biomes_.empty()) {
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        layer[x][z] = this->biomes_[0][x * 16 + z];
                    }
                }
            }
            return layer;
        }
        y -= dimension_min_y(this->pos_.dim);
        if (y < 0 || y >= static_cast<int>(this->biomes_.size())) {
            return {};
        }
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                layer[x][z] = this->biomes_[y][x * 16 + z];
            }
        }
        return layer;
    }

    biome biome3d::get_top_biome(int cx, int cz) {
        if (!this->use_3d_biome_maps_) return this->get_biome(cx, 0, cz);
        int y = (int)this->biomes_.size() - 1;
        while (y >= 0 && this->biomes_[y][cx * 16 + cz] == none) {
            y--;
        }
        return y < 0 ? biome::none : this->biomes_[y][cx * 16 + cz];
    }
    bool biome3d::load_from_d2d(const byte_t* data, size_t len) {
        if (len != 768) {  // height map: 512bytes biome: 256 bytes
            LOG_F(ERROR, "Invalid Data2d format (%zu)", len);
            return false;
        }
        memcpy(this->height_map_.data(), data, 512);
        this->use_3d_biome_maps_ = false;
        std::array<biome, 256> layer{};
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                layer[x * 16 + z] = static_cast<biome>(data[512 + x + 16 * z]);
            }
        }
        this->biomes_.push_back(layer);
        return true;
    }

    void biome3d::set_all(biome b) {
        for (auto& layer : biomes_) {
            std::fill(layer.begin(), layer.end(), b);
        }
    }

    void biome3d::set_height(int x, int z, int world_y) {
        // Inverse of height(): Data2D carries no Y anchor, so its rows stay world-space.
        const int my = this->use_3d_biome_maps_ ? dimension_min_y(this->pos_.dim) : 0;
        this->height_map_[x + z * 16] = static_cast<int16_t>(world_y - my);
    }

    std::string biome3d::to_raw() const {
        if (!this->use_3d_biome_maps_) {
            std::string result(512 + 256, '\0');
            memcpy(result.data(), height_map_.data(), 512);
            if (!biomes_.empty()) {
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        result[512 + x + 16 * z] = static_cast<char>(biomes_[0][x * 16 + z]);
                    }
                }
            }
            return result;
        }

        std::string result;
        result.reserve(512 + biomes_.size() * 5);
        result.append(reinterpret_cast<const char*>(height_map_.data()), 512);

        const size_t layer_count = biomes_.size();
        for (size_t sc = 0; sc * 16 < layer_count; ++sc) {
            std::vector<biome> values(4096, biome::none);
            for (size_t y = 0; y < 16 && sc * 16 + y < layer_count; ++y) {
                const auto& layer = biomes_[sc * 16 + y];
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        values[x * 256 + z * 16 + y] = layer[x * 16 + z];
                    }
                }
            }
            append_packed_biome_sub_chunk(result, values);
        }
        return result;
    }

}  // namespace bl
