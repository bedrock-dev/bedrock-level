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
        block_info get_block(int cx, int y, int cz);

        biome get_biome(int cx, int y, int cz);

        std::array<std::array<biome, 16>, 16> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        std::vector<std::string> &get_actor_list() {
            return this->actor_digest_list_.actor_digests_;
        }

        int get_height(int cx, int cz);

        explicit chunk(const chunk_pos &pos) : pos_(pos), loaded_(false){};

        chunk() = delete;

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }

       private:
        bool load_subchunks_(bedrock_level &level);

        bool load_d3d(bedrock_level &level);

        bool load_actor_digest(bedrock_level &level);

        void load_data(bedrock_level &level);

        bool loaded_{false};
        const chunk_pos pos_;
        // sub_chunks
        std::map<int, sub_chunk> sub_chunks_;
        // biome and height map
        data_3d d3d_{};
        // actor digest
        bl::actor_digest_list actor_digest_list_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
