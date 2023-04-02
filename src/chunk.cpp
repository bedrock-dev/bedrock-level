//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"
#include "bedrock_level.h"
#include "bedrock_key.h"
#include "utils.h"
#include <utility>

namespace bl {


    /**
     * Overworld [-64 ~-1]+[0~319]
     * [-64,-49][-48,-33][-32,-17][-16,-1]
     * NEther  [0~127]
     * The End [0~255]
     */



    void chunk::map_y_to_subchunk(int y, int &index, int &offset) {
        index = y < 0 ? (y - 15) / 16 : y / 16;
        offset = y % 16;
        if (offset < 0)offset += 16;
    }

    block_info bl::chunk::get_block(int cx, int y, int cz) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return {};
        }
        return it->second.get_block(cx, offset, cz);
    }

    bool chunk::load_data(bedrock_level &level) {
        if (this->loaded())return true;
        BL_LOGGER("Try load chunk %s", this->pos_.to_string().c_str());
        auto &db = level.db();
        for (auto sub_index: this->sub_chunk_indexes_) {
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
