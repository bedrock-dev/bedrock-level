//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bedrock_key.h"
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

        mcstructure() = default;
        mcstructure(const mcstructure &) = delete;
        mcstructure &operator=(const mcstructure &) = delete;
        mcstructure(mcstructure &&) noexcept = default;
        mcstructure &operator=(mcstructure &&) noexcept = default;
        ~mcstructure();

        [[nodiscard]] int size_x() const noexcept { return size_x_; }
        [[nodiscard]] int size_y() const noexcept { return size_y_; }
        [[nodiscard]] int size_z() const noexcept { return size_z_; }
        [[nodiscard]] block_pos size() const noexcept { return {size_x_, size_y_, size_z_}; }
        [[nodiscard]] int32_t version() const noexcept { return version_; }

        [[nodiscard]] block_pos origin() const noexcept { return origin_; }

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
        [[nodiscard]] block_pos block_entity_position(size_t index) const noexcept;

        [[nodiscard]] int32_t block_index(int layer, int x, int y, int z) const noexcept;
        [[nodiscard]] int32_t block_index(int x, int y, int z) const noexcept;
        [[nodiscard]] const palette_entry *block_at(int layer, int x, int y, int z) const noexcept;
        [[nodiscard]] const palette_entry *block_at(int x, int y, int z) const noexcept;

        [[nodiscard]] std::string to_raw() const;
        [[nodiscard]] bool save_to_file(const std::string &file_name) const;

        // readable summary of the parsed structure
        std::string dump() const;

       private:
        friend mcstructure parse_mcstructure(const byte_t *data, size_t len);
        friend class mcstructure_builder;

        int size_x_{0}, size_y_{0}, size_z_{0};
        int32_t version_{0};
        std::vector<palette_entry> palette_;  // block states from the "default" palette; owns tags
        layer_type layers_[2];                // block index per layer (ZYX order); -1 = void
        block_pos origin_;
        std::vector<bl::nbt::compound_tag *> entities_;        // owned
        std::vector<bl::nbt::compound_tag *> block_entities_;  // owned
        std::vector<block_pos> block_entity_positions_;        // local position of block entitiy
    };

    // Parse a .mcstructure file from raw bytes.
    mcstructure parse_mcstructure(const byte_t *data, size_t len);

    class mcstructure_builder {
       public:
        // origin is the structure_world_origin metadata used when the structure is loaded back into a world.
        mcstructure_builder(const block_pos &size, const block_pos &origin, int32_t version = 1);

        mcstructure_builder &set_block(const block_pos &pos, const bl::nbt::compound_tag *tag);
        mcstructure_builder &set_block(int layer, const block_pos &pos, const bl::nbt::compound_tag *tag);

        mcstructure_builder &fill_blocks(const block_box &box, const bl::nbt::compound_tag *tag);
        mcstructure_builder &fill_blocks(int layer, const block_box &box, const bl::nbt::compound_tag *tag);

        mcstructure_builder &set_block_entity(const block_pos &pos, const bl::nbt::compound_tag *tag);

        /// Add an entity whose NBT coordinates are absolute world coordinates.
        mcstructure_builder &add_entity(const bl::nbt::compound_tag *tag);

        [[nodiscard]] mcstructure build();

       private:
        using layer_type = mcstructure::layer_type;

        [[nodiscard]] static size_t flat_index(int x, int y, int z, int size_y, int size_z);
        [[nodiscard]] block_pos size() const noexcept { return {size_x_, size_y_, size_z_}; }
        [[nodiscard]] bool has_size() const noexcept { return size_x_ > 0 && size_y_ > 0 && size_z_ > 0; }
        void reset_layers();
        [[nodiscard]] layer_type &layer_at(int layer);
        [[nodiscard]] const layer_type &layer_at(int layer) const;
        [[nodiscard]] size_t ensure_palette_index(bl::nbt::compound_tag *tag);
        [[nodiscard]] bl::nbt::compound_tag *intern_tag(const bl::nbt::compound_tag *tag, bool strip_version);
        void release_ownership();

        int size_x_{0}, size_y_{0}, size_z_{0};
        block_pos origin_;
        int32_t version_{1};
        layer_type layers_[2];

        std::vector<palette_entry> palette_;
        std::unordered_map<std::string, size_t> palette_index_by_raw_;
        std::vector<bl::nbt::compound_tag *> block_entities_;
        std::vector<block_pos> block_entity_positions_;
        std::vector<bl::nbt::compound_tag *> entities_;

        std::unordered_map<std::string, bl::nbt::compound_tag *> interned_tags_;
        std::vector<std::unique_ptr<bl::nbt::compound_tag>> owned_tags_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_PALETTE_H
