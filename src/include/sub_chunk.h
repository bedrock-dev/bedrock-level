//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_SUB_CHUNK_H
#define BEDROCK_LEVEL_SUB_CHUNK_H

#include <cstdint>
#include <vector>

#include "bedrock_key.h"
#include "color.h"
#include "nbt.h"
#include "palette.h"

namespace bl {
    // On-disk layout version of a SubChunkTerrain payload, stored as its first byte.
    // read_header() rejects everything outside this set.
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

            /// Writes one block using the same (rx, ry, rz) convention as get_block_name.
            /// An untouched layer is seeded with air first, so positions that are never written
            /// read back as air rather than as whichever block happened to be written first.
            /// Palette entries are appended and NOT deduplicated -- call compact() before writing.
            void set_block(int rx, int ry, int rz, const nbt::compound_tag* tag);

            /// Fills the whole layer with one block, discarding the previous palette. The
            /// result is a uniform layer (bits == 0) once compact() has run.
            void fill_blocks(const nbt::compound_tag* tag);

            /// Fills the part of the layer inside box (sub-chunk-local coordinates, clipped to
            /// 0..15) with one block. Unlike the whole-layer overload this APPENDS a palette
            /// entry and leaves the rest of the layer alone, matching set_block. compact() drops
            /// whatever the overwrite made unreferenced.
            void fill_blocks(const block_box& box, const nbt::compound_tag* tag);

            /// Rebuilds the palette from the entries the blocks actually reference, dropping
            /// duplicates and entries nothing points at. This is the write-prep step: afterwards
            /// the palette describes exactly what the layer holds, so a layer that was cleared
            /// down to one block serializes as uniform (bits == 0).
            void compact();

            ~layer();
        };

        sub_chunk() = default;
        ~sub_chunk();

        bool load(const byte_t* data, size_t len);

        /// Serialize back into a SubChunkTerrain payload. The cached bits / palette_len layer
        /// fields are ignored; both are recomputed from the palette while writing.
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

        /// Writes one block, creating the target layer (and any layers below it) when needed.
        /// Same append-only rule as layer::set_block: deduplicate with compact() afterwards.
        void set_block(int rx, int ry, int rz, const nbt::compound_tag* tag, int layer_index = 0);

        /// Fills whole layers with one block, discarding their previous palettes.
        /// layer_index < 0 fills every existing layer and does nothing when there are none;
        /// otherwise the target layer is created as uniform air if it does not exist yet.
        void fill_layer(const nbt::compound_tag* tag, int layer_index = -1);

        /// Fills the part of the given layers inside box (sub-chunk-local, left-closed
        /// right-open) with one block, leaving everything outside the box alone.
        /// layer_index behaves as in fill_layer.
        void fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer_index = -1);

        /// Rebuilds every layer so its palette holds exactly the blocks in use. Editing appends
        /// palette entries, so this is what restores the compact on-disk form before writing;
        /// without it the output stays valid but can be orders of magnitude larger.
        /// Invalidates any nbt::compound_tag* previously returned by get_block_raw().
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
