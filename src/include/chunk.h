//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

// cached chunks

#include <unordered_map>
#include <unordered_set>

#include "actor.h"
#include "bedrock_key.h"
#include "color.h"
#include "data_3d.h"
#include "sub_chunk.h"
namespace bl {

    class bedrock_level;

    class chunk {
       public:
        friend class bedrock_level;
        static bool valid_in_chunk_pos(int cx, int y, int cz, int dim);
        static void map_y_to_subchunk(int y, int &index, int &offset);

       public:
        [[nodiscard]] inline bool fast_load() const { return this->fast_load_mode_; }

        block_info get_block(int cx, int y, int cz);

        block_info get_block_fast(int cx, int y, int cz);

        //        block_info get_top_block(int cx, int cz);

        palette::compound_tag *get_block_raw(int cx, int y, int cz);

        //        palette::compound_tag *get_top_block_raw(int cx, int cz);

        bl::color get_block_color(int cx, int y, int cz);

        //        bl::color get_top_block_color(int cx, int cz);

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        [[nodiscard]] bl::chunk_pos get_pos() const;

        int get_height(int cx, int cz);

        explicit chunk(const chunk_pos &pos) : loaded_(false), pos_(pos){};

        chunk() = delete;

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }
        std::vector<bl::palette::compound_tag *> &block_entities() { return this->block_entities_; }
        std::vector<bl::palette::compound_tag *> &pending_ticks() { return this->pending_ticks_; }

        std::vector<bl::actor *> entities() & { return this->entities_; }

        std::vector<hardcoded_spawn_area> HSAs() { return this->HSAs_; }

        [[nodiscard]] ChunkVersion get_version() const { return this->version; }
        ~chunk();

       private:
        bool load_data(bedrock_level &level, bool fast_load);

       private:
        bool load_subchunks(bedrock_level &level);

        bool load_biomes(bedrock_level &level);

        void load_entities(bedrock_level &level);

        bool load_pending_ticks(bedrock_level &level);

        bool load_block_entities(bedrock_level &level);

        void load_hsa(bedrock_level &level);

        bool loaded_{false};
        const chunk_pos pos_;
        // sub_chunks
        std::map<int, sub_chunk *> sub_chunks_;
        // biome and height map
        biome3d d3d_{};
        // actor digest
        //        bl::actor_digest_list actor_digest_list_;
        // block entities
        std::vector<bl::actor *> entities_;
        std::vector<bl::palette::compound_tag *> block_entities_;
        std::vector<bl::palette::compound_tag *> pending_ticks_;

        std::vector<bl::hardcoded_spawn_area> HSAs_;
        ChunkVersion version{New};
        bool fast_load_mode_{false};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
