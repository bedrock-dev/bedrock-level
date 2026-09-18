//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include "loguru/loguru.hpp"

namespace bl {
    namespace {
        constexpr auto BLOCK_NUM = 16 * 16 * 16;
    }  // namespace

    std::vector<uint16_t> read_block_indices(const byte_t* stream, int& read, uint8_t& bits, uint32_t& palette_len) {
        read = 0;
        auto layer_header = stream[0];
        read++;
        bits = layer_header >> 1u;
        std::vector<uint16_t> blocks;
        if (bits != 0) {
            blocks.resize(BLOCK_NUM);
            int block_per_word = 32 / bits;
            auto wordCount = BLOCK_NUM / block_per_word;
            if (BLOCK_NUM % block_per_word != 0) wordCount++;
            int position = 0;
            for (int wordi = 0; wordi < wordCount; wordi++) {
                auto word = *reinterpret_cast<const int*>(stream + read + wordi * 4);
                for (int block = 0; block < block_per_word; block++) {
                    int state = (word >> ((position % block_per_word) * bits)) & ((1 << bits) - 1);
                    if (position < static_cast<int>(blocks.size())) {
                        blocks[position] = static_cast<uint16_t>(state);
                    }
                    position++;
                }
            }
            read += wordCount << 2;
            palette_len = *reinterpret_cast<const int*>(stream + read);
            read += 4;
        } else {  // uniform
            blocks = std::vector<uint16_t>(BLOCK_NUM, 0);
            palette_len = 1;
        }
        return blocks;
    }

    std::vector<palette_entry> read_palettes(const byte_t* stream, size_t number, size_t len, int& read) {
        read = 0;
        std::vector<palette_entry> result;
        result.reserve(number);
        for (auto i = 0u; i < number; i++) {
            int r = 0;
            auto* tag = bl::nbt::read_one_palette(stream + read, len - read, r);
            if (tag) {
                result.push_back(make_palette_entry(tag));
            } else {
                LOG_F(ERROR, "Can not read block palette");
                return result;
            }
            read += r;
        }
        return result;
    }

    void write_layer(std::string& out, const std::vector<uint16_t>& blocks, const std::vector<palette_entry>& palette) {
        // bits has to cover every palette index; a 1-entry palette collapses to the uniform form
        uint8_t bits = 0;
        while ((1u << bits) < palette.size()) bits++;

        // Bit 0 of the header is the layer type: vanilla writes 0 (palette-based) on every layer.
        out.push_back(static_cast<char>(bits << 1));
        if (bits != 0) {
            const int per_word = 32 / bits;
            const int words = (BLOCK_NUM + per_word - 1) / per_word;
            for (int word_index = 0; word_index < words; word_index++) {
                uint32_t word = 0;
                for (int slot = 0; slot < per_word; slot++) {
                    const int position = word_index * per_word + slot;
                    if (position >= BLOCK_NUM) break;
                    const uint32_t index = position < static_cast<int>(blocks.size()) ? blocks[position] : 0;
                    word |= index << (slot * bits);
                }
                for (int byte = 0; byte < 4; byte++) out.push_back(static_cast<char>((word >> (byte * 8)) & 0xff));
            }
            const int32_t palette_len = static_cast<int32_t>(palette.size());
            for (int byte = 0; byte < 4; byte++) out.push_back(static_cast<char>((palette_len >> (byte * 8)) & 0xff));
        }

        for (const auto& entry : palette) {
            if (entry.tag) out += entry.tag->to_raw();
        }
    }

    palette_entry make_palette_entry(bl::nbt::compound_tag* tag) {
        palette_entry entry;
        entry.tag = tag;
        tag->remove("version");  // remove version tag(compatibility for color table)
        // pre-resolve block name so per-block lookups become O(1) indexing
        std::string name{"minecraft:unknown"};
        if (auto* name_tag = tag->get("name"); name_tag) {
            if (auto* st = name_tag->as<bl::nbt::string_tag*>(); st) {
                name = st->value;
            }
        }
        entry.name = std::move(name);
        return entry;
    }
}  // namespace bl
