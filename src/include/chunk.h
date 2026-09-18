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
        /// is missing. New sub-chunks inherit this chunk's format, so the Y range that
        /// chunk_format() selects stays consistent.
        /// Palette entries are appended, not deduplicated: compact() before writing the terrain out.
        void set_block(int cx, int y, int cz, const nbt::compound_tag* tag, int layer = 0);

        /// Writes one block entity at the chunk-local position (cx, y, cz). The tag is cloned and
        /// its x/y/z are rewritten to the matching world position, then it is appended to
        /// block_entities_. A second write to the same position leaves both entries; compact()
        /// drops the older one.
        void set_block_entity(int cx, int y, int cz, const nbt::compound_tag* tag);

        /// Fills the blocks inside box with one block. x/z are chunk-local and clipped to 0..15,
        /// y is a world coordinate whose span may cross sub-chunk boundaries; the box itself is
        /// left-closed right-open like every other block_box.
        /// layer >= 0 creates whatever is missing so the fill always lands; layer < 0 only
        /// touches sub-chunks and layers that already exist.
        void fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer = -1);

        /// Adds an entity to this chunk, placing it at the world position world_pos. The tag is
        /// cloned, so the caller keeps ownership of it and may destroy it right away. The clone
        /// gets a freshly generated unique id -- both UniqueID and the storage key the level
        /// indexes the actor by -- because reusing the id of the actor the tag came from would
        /// make the new one and the old one collide.
        ///
        /// The tag's own Pos is overwritten rather than shifted, so the result does not depend on
        /// it: a caller moving a whole structure has to add that translation to each entity's
        /// position itself.
        ///
        /// Returns false when tag is not a loadable actor (it needs Pos, identifier and
        /// UniqueID), in which case the chunk is left untouched.
        bool add_actor(bedrock_level& level, const nbt::compound_tag* tag, const vec3& world_pos);

        /// Rebuilds the chunk into its compact on-disk form: every sub-chunk's palette is
        /// deduplicated, block_entities_ is collapsed to the last write per position, and the
        /// biome/height payload's height map is recomputed from the terrain, so block edits are
        /// reflected in it. Entity compaction would belong here too.
        /// Invalidates any nbt::compound_tag* previously returned by get_block_raw().
        void compact();

        /// Serializes this chunk into a fresh raw_chunk: the version marker, the finalized state,
        /// the sub-chunk payloads, the biome/height payload (whose height map has been recomputed
        /// from the terrain) and, when they were part of the load (chunk_load_policy::Actor /
        /// BlockActor), the block entities and entities. Keys the chunk never read are left out,
        /// which also keeps raw_chunk::write from touching them in the level.
        ///
        /// Compacts first: editing appends palette entries and can leave several block entities
        /// on one position, and skipping that would still write valid data but could inflate a
        /// sub-chunk from tens of bytes to ~100 KB. Compacting is therefore not const, and it
        /// invalidates nbt::compound_tag* values from get_block_raw().
        [[nodiscard]] raw_chunk to_raw_chunk();

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

        /// On-disk format of the chunk, taken from the raw_chunk it was loaded from. Also picks
        /// the sub-chunk layout that new sub-chunks inherit.
        [[nodiscard]] LevelChunkFormat chunk_format() const { return this->chunk_format_; }

       public:
        bool load_from_raw_chunk(const bl::raw_chunk& rc, chunk_load_policy policy = chunk_load_policy::All);

        ~chunk();

       private:
        bool load_data(bedrock_level& level, chunk_load_policy policy);

        /// Sub-chunk containing y, created (empty) when the chunk has none at that Y index.
        [[nodiscard]] sub_chunk* ensure_sub_chunk(int y);

        /// Recomputes the stored height map from the blocks the chunk currently holds.
        void refresh_height_map();

       private:
        bool load_subchunks(const bl::raw_chunk& rc);

        bool load_biomes(const bl::raw_chunk& rc);

        void load_entities(const bl::raw_chunk& rc);

        bool load_pending_ticks(const bl::raw_chunk& rc);

        bool load_block_entities(const bl::raw_chunk& rc);

        void load_hsa(const bl::raw_chunk& rc);

        bool loaded_{false};
        const chunk_pos pos_;
        // true once the matching payload has been read; see to_raw_chunk()
        bool block_entities_loaded_{false};
        bool entities_loaded_{false};
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
        LevelChunkFormat chunk_format_{LevelChunkFormat::V1_18_3IndividualActorStorage};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
