//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H


//cached chunks

#include "sub_chunk.h"
#include <unordered_map>
#include "bedrock_key.h"
#include <unordered_set>

namespace bl {

    class bedrock_level;


    class chunk {

    public:

        friend class bedrock_level;

        block_info get_block(int x, int y, int z);

        explicit chunk(const chunk_pos &pos) : pos_(pos), loaded_(false) {};

        chunk() = delete;

        inline bool loaded() const { return this->loaded_; }

    private:

        bool load_data(bedrock_level &level);

        bool loaded_{false};
        std::map<int8_t, sub_chunk> sub_chunks_;

        //lighted
        const chunk_pos pos_;
        std::unordered_set<int8_t> sub_chunk_indexes_;
    };
}


#endif //BEDROCK_LEVEL_CHUNK_H
