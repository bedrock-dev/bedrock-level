#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "nbt.h"

namespace bl {
    // Read the layer header and 4096 block indices.
    std::vector<uint16_t> read_block_indices(const byte_t* stream, int& read, uint8_t& bits, uint32_t& palette_len);

    struct palette_entry {
        bl::nbt::compound_tag* tag = nullptr;
        std::string name{"minecraft:unknown"};
    };

    // Read palette entries and their resolved names.
    std::vector<palette_entry> read_palettes(const byte_t* stream, size_t number, size_t len, int& read);

    // Write a complete layer body, recomputing bits from the palette.
    void write_layer(std::string& out, const std::vector<uint16_t>& blocks, const std::vector<palette_entry>& palette);

    // Build a palette entry from a parsed block-state compound.
    palette_entry make_palette_entry(bl::nbt::compound_tag* tag);
}  // namespace bl

#endif  // BEDROCK_LEVEL_PALETTE_H
