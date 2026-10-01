#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <cstdint>
#include <vector>

#include "bedrock_key.h"
#include "color.h"
#include "nbt.h"
#include "palette.h"

namespace bl {
    // On-disk SubChunkTerrain version byte.
    enum class SubChunkVersion : uint8_t {
        V8 = 8,  // 1.2~1.17: header is version + layer count
        V9 = 9   // 1.18+: header also carries a Y index
    };

    /// Marker for a sub_chunk that was built rather than loaded: it has no version byte yet.
    inline constexpr uint8_t UNSET_SUB_CHUNK_VERSION = 0xff;

    [[nodiscard]] constexpr bool is_supported_sub_chunk_version(uint8_t version) noexcept {
        return version == static_cast<uint8_t>(SubChunkVersion::V8) || version == static_cast<uint8_t>(SubChunkVersion::V9);
    }

    struct block_appearance {
        std::string name{"minecraft:unknown"};
        bl::color color{173, 8, 172, 255};
    };

    class sub_chunk {
       public:
        /// One block layer: a palette plus a palette index per block position.
        struct layer {
            layer() = default;
            uint8_t bits{};
            uint8_t type{};
            uint32_t palette_len{};
            std::vector<uint16_t> blocks{};
            std::vector<palette_entry> palette;

            /// Write a block; untouched positions remain air.
            void set_block(int rx, int ry, int rz, const nbt::compound_tag* tag);

            /// Fill the entire layer with one block.
            void fill_blocks(const nbt::compound_tag* tag);

            /// Fill a clipped sub-chunk-local box with one block.
            void fill_blocks(const block_box& box, const nbt::compound_tag* tag);

            /// Remove unused palette entries and rebuild indices.
            void compact();

            ~layer();
        };

        sub_chunk() = default;
        ~sub_chunk();

        bool load(const byte_t* data, size_t len);

        /// Serialize to a SubChunkTerrain payload.
        [[nodiscard]] std::string to_raw() const;

        void set_version(SubChunkVersion version) { this->version_ = static_cast<uint8_t>(version); }
        /// Raw on-disk version byte; UNSET_SUB_CHUNK_VERSION until load() or set_version() runs.
        [[nodiscard]] inline uint8_t version() const { return this->version_; }

        void set_y_index(int8_t y_index) { this->y_index_ = y_index; }
        [[nodiscard]] inline int8_t y_index() const { return this->y_index_; }

        /// Block name without copying (lives as long as the sub_chunk); "minecraft:unknown" on miss
        [[nodiscard]] const std::string& get_block_name(int rx, int ry, int rz, int layer);

        nbt::compound_tag* get_block_raw(int rx, int ry, int rz, int layer);

        block_appearance get_block_with_color(int rx, int ry, int rz, int layer);

        /// Write a block, creating missing layers as needed.
        void set_block(int rx, int ry, int rz, const nbt::compound_tag* tag, int layer_index = 0);

        /// Fill whole layers with one block.
        void fill_layer(const nbt::compound_tag* tag, int layer_index = -1);

        /// Fill a left-closed/right-open box in the selected layers.
        void fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer_index = -1);

        /// Compact every layer before writing.
        void compact();

       private:
        void push_back_layer(layer* layer) { this->layers_.push_back(layer); }

        /// Layer at index, appending uniform-air layers until it exists; nullptr when index < 0.
        [[nodiscard]] layer* ensure_layer(int index);

        // Shared palette lookup for the get_block* accessors; nullptr when coords/layer/index are invalid.
        [[nodiscard]] const palette_entry* palette_entry_at(int rx, int ry, int rz, int layer) const;

        uint8_t version_{UNSET_SUB_CHUNK_VERSION};
        int8_t y_index_{0};
        std::vector<layer*> layers_;
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_SUB_CHUNK_H
