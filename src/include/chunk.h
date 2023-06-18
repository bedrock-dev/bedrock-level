//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

// cached chunks

#include <unordered_map>
#include <unordered_set>

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

        int get_height(int cx, int cz);

        explicit chunk(const chunk_pos &pos) : pos_(pos), loaded_(false){};

        chunk() = delete;

        inline bool loaded() const { return this->loaded_; }

       private:
        void load_data(bedrock_level &level);

        bool loaded_{false};
        std::map<int, sub_chunk> sub_chunks_;
        // lighted
        const chunk_pos pos_;
        std::unordered_set<int8_t> sub_chunk_indexes_;
        data_3d d3d_{};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
