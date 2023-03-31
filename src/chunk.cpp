//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"
#include "bedrock_level.h"
#include "bedrock_key.h"

namespace bl {

    BlockInfo bl::chunk::get_block(int x, int y, int z) {
        return {};
    }

    bool chunk::load_data(bedrock_level &level) {
        auto &db = level.db();
        //get sub chunks terrain
        for (auto sub_index: this->sub_chunk_indexes_) {
            auto terrain_key = bl::chunk_key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            std::string raw;
            auto r = db->Get(leveldb::ReadOptions(), terrain_key.to_raw(), &raw);
            if (!r.ok()) {
                throw std::runtime_error("Can not read sub chunk content. " + terrain_key.to_string());
            }

            bl::sub_chunk sb;
            if (!sb.load(reinterpret_cast<uint8_t *> (raw.data()), raw.size())) {
                throw std::runtime_error("Can not parse sub chunk content. " + terrain_key.to_string());
            }

            this->sub_chunks_[sub_index] = sb;
        }
        //TODO: load others (actor data3d block entities.etc)
        loaded_ = true;
        return true;
    }
}
