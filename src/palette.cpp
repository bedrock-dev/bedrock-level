#include "palette.h"

#include <limits>

#include "binary_io.h"
#include "loguru/loguru.hpp"

namespace bl {
    namespace {
        constexpr auto BLOCK_NUM = 16 * 16 * 16;
    }  // namespace

    std::vector<uint16_t> read_block_indices(std::span<const byte_t> stream, int& read, uint8_t& bits, uint32_t& palette_len) {
        read = 0;
        palette_len = 0;
        if (stream.empty()) return {};
        const auto* data = stream.data();
        auto layer_header = data[0];
        read++;
        bits = layer_header >> 1u;
        if (bits > 32) return {};
        std::vector<uint16_t> blocks;
        if (bits != 0) {
            blocks.resize(BLOCK_NUM);
            const int blocks_per_word = 32 / bits;
            size_t word_count = BLOCK_NUM / blocks_per_word;
            if (BLOCK_NUM % blocks_per_word != 0) word_count++;
            const auto required = 1 + word_count * sizeof(uint32_t) + sizeof(uint32_t);
            if (stream.size() < required) return {};
            int position = 0;
            const uint32_t mask = bits == 32 ? std::numeric_limits<uint32_t>::max() : (1u << bits) - 1u;
            for (size_t word_index = 0; word_index < word_count; word_index++) {
                const auto word = binary::read_u32_le(data + read + word_index * 4);
                for (int block = 0; block < blocks_per_word; block++) {
                    uint32_t state = (word >> ((position % blocks_per_word) * bits)) & mask;
                    if (position < static_cast<int>(blocks.size())) {
                        blocks[position] = static_cast<uint16_t>(state);
                    }
                    position++;
                }
            }
            read += static_cast<int>(word_count * sizeof(uint32_t));
            palette_len = binary::read_u32_le(data + read);
            read += sizeof(uint32_t);
        } else {  // uniform
            blocks = std::vector<uint16_t>(BLOCK_NUM, 0);
            palette_len = 1;
        }
        return blocks;
    }

    std::vector<palette_entry> read_palettes(std::span<const byte_t> stream, size_t number, int& read) {
        read = 0;
        std::vector<palette_entry> result;
        result.reserve(number);
        for (auto i = 0u; i < number; i++) {
            if (static_cast<size_t>(read) >= stream.size()) {
                LOG_F(ERROR, "Block palette data ended unexpectedly");
                return result;
            }
            const auto parsed = bl::nbt::parse_one(stream.subspan(static_cast<size_t>(read)));
            if (!parsed) {
                LOG_F(ERROR, "Can not read block palette: %s", bl::nbt::parse_error_to_string(parsed.error_code));
                return result;
            }
            if (parsed.consumed_length == 0 || parsed.consumed_length > stream.size() - static_cast<size_t>(read)) {
                LOG_F(ERROR, "Invalid block palette length");
                delete parsed.root;
                return result;
            }
            result.push_back(make_palette_entry(parsed.root));
            read += static_cast<int>(parsed.consumed_length);
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
                binary::append_u32_le(out, word);
            }
            const int32_t palette_len = static_cast<int32_t>(palette.size());
            binary::append_i32_le(out, palette_len);
        }

        for (const auto& entry : palette) {
            if (entry.tag) out += entry.tag->serialize();
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
