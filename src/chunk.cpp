//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"
#include "bedrock_level.h"
#include "bedrock_key.h"
#include "utils.h"


namespace bl {

    namespace {

        int8_t get_y_index(int y) {
            return y / 16;
        }
    }

    block_info bl::chunk::get_block(int x, int y, int z) {
        auto idx = y / 16;
        auto it = this->sub_chunks_.find((int8_t) idx);
        if (it == this->sub_chunks_.end()) {
            return {"minecraft:air"};
        }
        return it->second.get_block(x, y % 16, z);
    }

    bool chunk::load_data(bedrock_level &level) {
        auto &db = level.db();
        //get sub chunks terrain
        for (auto sub_index: this->sub_chunk_indexes_) {
//            BL_LOGGER("Sub index is %d", sub_index);
            auto terrain_key = bl::chunk_key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            std::string raw;
            auto r = db->Get(leveldb::ReadOptions(), terrain_key.to_raw(), &raw);
            if (!r.ok()) {
                throw std::runtime_error("Can not read sub chunk content. " + terrain_key.to_string());
            }

            bl::sub_chunk sb;
            if (!sb.load(raw.data(), raw.size())) {
                throw std::runtime_error("Can not parse sub chunk content. " + terrain_key.to_string());
            }

            this->sub_chunks_[sub_index] = sb;
        }
        //TODO: load others (actor data3d block entities.etc)
        loaded_ = true;
        return true;
    }
}
