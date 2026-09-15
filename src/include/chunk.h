//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

// cached chunks

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "actor.h"
#include "bedrock_key.h"
#include "data_3d.h"
#include "raw_chunk.h"
#include "sub_chunk.h"

namespace bl {

    class bedrock_level;

    class chunk {
       public:
        friend class bedrock_level;
        static void map_y_to_subchunk(int y, int& index, int& offset);

       public:
        block_appearance get_block_with_color(int cx, int y, int cz, int layer = 0);

        /// Block name without copying (lives as long as the chunk); "minecraft:unknown" on miss
        [[nodiscard]] const std::string& get_block_name(int cx, int y, int cz, int layer = 0);

        nbt::compound_tag* get_block_raw(int cx, int y, int cz, int layer = 0);

        /// Writes one block, creating the target layer and the sub-chunk holding it when either
        /// is missing. New sub-chunks inherit this chunk's layout version, so the Y range that
        /// get_version() selects stays consistent.
        /// Palette entries are appended, not deduplicated: compact() before writing the terrain out.
        void set_block(int cx, int y, int cz, const nbt::compound_tag* tag, int layer = 0);

        /// Fills the blocks inside box with one block. x/z are chunk-local and clipped to 0..15,
        /// y is a world coordinate whose span may cross sub-chunk boundaries; the box itself is
        /// left-closed right-open like every other block_box.
        /// layer >= 0 creates whatever is missing so the fill always lands; layer < 0 only
        /// touches sub-chunks and layers that already exist.
        void fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer = -1);

        /// Rebuilds the chunk into its compact on-disk form. For now this forwards to every
        /// sub-chunk; entity and block-entity compaction would belong here too.
        /// Invalidates any nbt::compound_tag* previously returned by get_block_raw().
        void compact();

        /// Serializes the terrain into out, replacing out's SubChunkTerrain payloads. Everything
        /// else in out is left untouched, so a raw_chunk read with chunk_load_policy::All keeps
        /// its entities, block entities, ticks and HSA across a round trip.
        ///
        /// Compacts first: editing appends palette entries, and skipping that would still write
        /// valid data but could inflate a sub-chunk from tens of bytes to ~100 KB. Compacting is
        /// therefore not const, and it invalidates nbt::compound_tag* values from get_block_raw().
        void to_raw_chunk(raw_chunk& out);

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        [[nodiscard]] bl::chunk_pos get_pos() const;

        /// World Y range actually covered by this chunk's terrain, derived from the sub-chunks
        /// it holds rather than from any version or dimension convention. Chunks are not limited
        /// to a fixed height, so this is the only correct source.
        /// Returns {0, -1} (an empty/inverted range) when the chunk has no terrain loaded.
        [[nodiscard]] std::pair<int, int> get_y_range() const;

        int get_height(int cx, int cz);

        std::pair<int, int> get_top_y(int cx, int cz, int max_y);

        explicit chunk(const chunk_pos& pos) : loaded_(false), pos_(pos) {};

        chunk() = delete;

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }
        std::vector<bl::nbt::compound_tag*>& block_entities() { return this->block_entities_; }
        std::vector<bl::nbt::compound_tag*>& pending_ticks() { return this->pending_ticks_; }

        std::vector<bl::actor*> entities() & { return this->entities_; }

        hardcoded_spawn_area_list& HSAs() { return this->HSAs_; }

        [[nodiscard]] ChunkVersion get_version() const { return this->version; }

       public:
        bool load_from_raw_chunk(const bl::raw_chunk& rc, chunk_load_policy policy = chunk_load_policy::All);

        ~chunk();

       private:
        bool load_data(bedrock_level& level, chunk_load_policy policy);

        /// Sub-chunk containing y, created (empty) when the chunk has none at that Y index.
        [[nodiscard]] sub_chunk* ensure_sub_chunk(int y);

       private:
        bool load_subchunks(const bl::raw_chunk& rc);

        bool load_biomes(const bl::raw_chunk& rc);

        void load_entities(const bl::raw_chunk& rc);

        bool load_pending_ticks(const bl::raw_chunk& rc);

        bool load_block_entities(const bl::raw_chunk& rc);

        void load_hsa(const bl::raw_chunk& rc);

        bool loaded_{false};
        const chunk_pos pos_;
        // sub_chunks
        std::map<int, sub_chunk*> sub_chunks_;
        // biome and height map
        biome3d d3d_{};
        // actor digest
        //        bl::actor_digest_list actor_digest_list_;
        // block entities
        std::vector<bl::actor*> entities_;
        std::vector<bl::nbt::compound_tag*> block_entities_;
        std::vector<bl::nbt::compound_tag*> pending_ticks_;

        bl::hardcoded_spawn_area_list HSAs_;
        ChunkVersion version{New};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
