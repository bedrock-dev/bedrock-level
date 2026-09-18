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
    std::vector<uint16_t> read_block_indices(const byte_t* stream, int& read, uint8_t& bits, uint32_t& palette_len);

    struct palette_entry {
        bl::nbt::compound_tag* tag = nullptr;
        std::string name{"minecraft:unknown"};
    };

    // Reads the palette list (second half). Returns palette entries (tag + pre-resolved name).
    std::vector<palette_entry> read_palettes(const byte_t* stream, size_t number, size_t len, int& read);

    // Writes the layer header + block index array + palette (the full layer body).
    // bits is recomputed from the palette size rather than taken from the layer's cached
    // field, and a single-entry palette is emitted as a uniform layer (no index array and
    // no palette_len field, matching what the game writes).
    void write_layer(std::string& out, const std::vector<uint16_t>& blocks, const std::vector<palette_entry>& palette);

    // Build a palette entry from a parsed block-state compound (shared by sub-chunk and mcstructure paths).
    palette_entry make_palette_entry(bl::nbt::compound_tag* tag);
}  // namespace bl

#endif  // BEDROCK_LEVEL_PALETTE_H
