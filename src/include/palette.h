//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
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
    class mcstructure {
       public:
        using layer_type = std::vector<int32_t>;
        using size_type = std::array<int, 3>;

        mcstructure() = default;
        mcstructure(const mcstructure &) = delete;
        mcstructure &operator=(const mcstructure &) = delete;
        mcstructure(mcstructure &&) noexcept = default;
        mcstructure &operator=(mcstructure &&) noexcept = default;
        ~mcstructure();

        [[nodiscard]] int size_x() const noexcept { return size_x_; }
        [[nodiscard]] int size_y() const noexcept { return size_y_; }
        [[nodiscard]] int size_z() const noexcept { return size_z_; }
        [[nodiscard]] size_type size() const noexcept { return {size_x_, size_y_, size_z_}; }

        [[nodiscard]] int origin_x() const noexcept { return origin_x_; }
        [[nodiscard]] int origin_y() const noexcept { return origin_y_; }
        [[nodiscard]] int origin_z() const noexcept { return origin_z_; }
        [[nodiscard]] size_type origin() const noexcept { return {origin_x_, origin_y_, origin_z_}; }

        [[nodiscard]] const std::vector<palette_entry> &palette() const noexcept { return palette_; }
        [[nodiscard]] size_t palette_size() const noexcept { return palette_.size(); }
        [[nodiscard]] const palette_entry *palette_entry_at(size_t index) const noexcept;

        [[nodiscard]] size_t layer_count() const noexcept { return 2; }
        [[nodiscard]] const layer_type &layer(size_t index) const noexcept;
        [[nodiscard]] size_t layer_size(size_t index) const noexcept;

        [[nodiscard]] const std::vector<bl::nbt::compound_tag *> &entities() const noexcept { return entities_; }
        [[nodiscard]] size_t entity_count() const noexcept { return entities_.size(); }
        [[nodiscard]] const std::vector<bl::nbt::compound_tag *> &block_entities() const noexcept { return block_entities_; }
        [[nodiscard]] size_t block_entity_count() const noexcept { return block_entities_.size(); }

        [[nodiscard]] int32_t block_index(int layer, int x, int y, int z) const noexcept;
        [[nodiscard]] int32_t block_index(int x, int y, int z) const noexcept;
        [[nodiscard]] const palette_entry *block_at(int layer, int x, int y, int z) const noexcept;
        [[nodiscard]] const palette_entry *block_at(int x, int y, int z) const noexcept;

        [[nodiscard]] std::string to_raw() const;
        bool save_to_file(const std::string &file_name) const;

        // readable summary of the parsed structure
        std::string dump() const;

       private:
        friend mcstructure parse_mcstructure(const byte_t *data, size_t len);
        friend class mcstructure_builder;

        int size_x_{0}, size_y_{0}, size_z_{0};
        std::vector<palette_entry> palette_;  // block states from the "default" palette; owns tags
        layer_type layers_[2];                // block index per layer (ZYX order); -1 = void
        int origin_x_{0}, origin_y_{0}, origin_z_{0};
        std::vector<bl::nbt::compound_tag *> entities_;        // owned
        std::vector<bl::nbt::compound_tag *> block_entities_;  // owned
    };

    // Parse a .mcstructure file from raw bytes.
    mcstructure parse_mcstructure(const byte_t *data, size_t len);

    class mcstructure_builder {
       public:
        mcstructure_builder() = default;

        mcstructure_builder &set_size(int x, int y, int z);
        // Sets structure_world_origin metadata used when the structure is loaded back into a world.
        mcstructure_builder &set_origin(int x, int y, int z);
        mcstructure_builder &set_world_origin(int x, int y, int z) { return set_origin(x, y, z); }

        mcstructure_builder &set_block(int x, int y, int z, const bl::nbt::compound_tag *tag);
        mcstructure_builder &set_block(int layer, int x, int y, int z, const bl::nbt::compound_tag *tag);

        mcstructure_builder &fill_blocks(int x0, int y0, int z0, int x1, int y1, int z1, const bl::nbt::compound_tag *tag);
        mcstructure_builder &fill_blocks(int layer, int x0, int y0, int z0, int x1, int y1, int z1, const bl::nbt::compound_tag *tag);

        mcstructure_builder &set_block_entity(int x, int y, int z, const bl::nbt::compound_tag *tag);

        [[nodiscard]] mcstructure build();

       private:
        using layer_type = mcstructure::layer_type;

        [[nodiscard]] static size_t flat_index(int x, int y, int z, int size_y, int size_z);
        [[nodiscard]] std::array<int, 3> size() const noexcept { return {size_x_, size_y_, size_z_}; }
        [[nodiscard]] bool has_size() const noexcept { return size_x_ > 0 && size_y_ > 0 && size_z_ > 0; }
        void reset_layers();
        [[nodiscard]] layer_type &layer_at(int layer);
        [[nodiscard]] const layer_type &layer_at(int layer) const;
        [[nodiscard]] size_t ensure_palette_index(bl::nbt::compound_tag *tag);
        [[nodiscard]] bl::nbt::compound_tag *intern_tag(const bl::nbt::compound_tag *tag, bool strip_version);
        [[nodiscard]] bl::nbt::compound_tag *intern_block_entity(const bl::nbt::compound_tag *tag, int x, int y, int z);
        void release_ownership();

        int size_x_{0}, size_y_{0}, size_z_{0};
        int origin_x_{0}, origin_y_{0}, origin_z_{0};
        layer_type layers_[2];

        std::vector<palette_entry> palette_;
        std::unordered_map<std::string, size_t> palette_index_by_raw_;
        std::vector<bl::nbt::compound_tag *> block_entities_;

        std::unordered_map<std::string, bl::nbt::compound_tag *> interned_tags_;
        std::vector<std::unique_ptr<bl::nbt::compound_tag>> owned_tags_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_PALETTE_H
