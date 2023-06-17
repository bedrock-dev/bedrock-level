//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

// cached chunks

#include <unordered_map>
#include <unordered_set>

#include "bedrock_key.h"
#include "sub_chunk.h"

namespace bl {

    class bedrock_level;

    class chunk {
       public:
        friend class bedrock_level;

        block_info get_block(int cx, int y, int cz);

        explicit chunk(const chunk_pos &pos) : pos_(pos), loaded_(false){};

        chunk() = delete;

        inline bool loaded() const { return this->loaded_; }

        static void map_y_to_subchunk(int y, int &index, int &offset);

       private:
        bool load_data(bedrock_level &level);

        bool loaded_{false};
        std::map<int, sub_chunk> sub_chunks_;

        // lighted
        const chunk_pos pos_;
        std::unordered_set<int8_t> sub_chunk_indexes_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
