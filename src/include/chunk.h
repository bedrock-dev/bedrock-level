#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

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

        /// Write a block, creating missing layers and sub-chunks.
        void set_block(int cx, int y, int cz, const nbt::compound_tag* tag, int layer = 0);

        /// Append a block entity after rewriting its world position.
        void set_block_entity(int cx, int y, int cz, const nbt::compound_tag* tag);

        /// Fill a clipped, left-closed/right-open box with one block.
        void fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer = -1);

        /// Clone and add an actor at `world_pos` with a fresh unique ID.
        bool add_actor(bedrock_level& level, const nbt::compound_tag* tag, const vec3& world_pos);

        /// Compact palettes, block entities, and derived height data before writing.
        void compact();

        /// Compact and serialize the loaded chunk payloads.
        [[nodiscard]] raw_chunk to_raw_chunk();

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        [[nodiscard]] bl::chunk_pos get_pos() const;

        /// Return the terrain's world Y range, or {0, -1} when empty.
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

        /// On-disk format and layout inherited by new sub-chunks.
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
        bool block_entities_loaded_{false};
        bool entities_loaded_{false};
        std::map<int, sub_chunk*> sub_chunks_;
        biome3d d3d_{};
        std::vector<bl::actor*> entities_;
        std::vector<bl::nbt::compound_tag*> block_entities_;
        std::vector<bl::nbt::compound_tag*> pending_ticks_;

        bl::hardcoded_spawn_area_list HSAs_;
        LevelChunkFormat chunk_format_{LevelChunkFormat::V1_18_3IndividualActorStorage};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
