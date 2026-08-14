//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "nbt.h"

namespace bl {
    // Reads the layer header + block index array (first half of a block-palette layer).
    // Returns the 4096 block indices; bits/palette_len are written back.
    std::vector<uint16_t> read_block_indices(const byte_t *stream, int &read, uint8_t &bits, uint32_t &palette_len);

    struct palette_entry {
        bl::nbt::compound_tag *tag = nullptr;
        std::string name{"minecraft:unknown"};
    };

    // Reads the palette list (second half). Returns palette entries (tag + pre-resolved name).
    std::vector<palette_entry> read_palettes(const byte_t *stream, size_t number, size_t len, int &read);

    // Build a palette entry from a parsed block-state compound (shared by sub-chunk and mcstructure paths).
    palette_entry make_palette_entry(bl::nbt::compound_tag *tag);

    // Parsed .mcstructure file (Bedrock structure block export). Owns all extracted NBT tags.
    struct mcstructure {
        int size_x{0}, size_y{0}, size_z{0};
        std::vector<palette_entry> palette;  // block states from the "default" palette; owns tags
        std::vector<int32_t> layers[2];      // block index per layer (ZYX order); -1 = void
        int origin_x{0}, origin_y{0}, origin_z{0};
        std::vector<bl::nbt::compound_tag *> entities;        // owned
        std::vector<bl::nbt::compound_tag *> block_entities;  // owned

        mcstructure() = default;
        mcstructure(const mcstructure &) = delete;
        mcstructure &operator=(const mcstructure &) = delete;
        mcstructure(mcstructure &&) noexcept = default;
        mcstructure &operator=(mcstructure &&) noexcept = default;
        ~mcstructure();

        // readable summary of the parsed structure
        std::string dump() const;
    };

    // Parse a .mcstructure file from raw bytes.
    mcstructure parse_mcstructure(const byte_t *data, size_t len);
}  // namespace bl

#endif  // BEDROCK_LEVEL_PALETTE_H
