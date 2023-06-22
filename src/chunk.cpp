//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"

#include <utility>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "utils.h"

namespace bl {

    /**
     * Overworld [-64 ~-1]+[0~319]
     * [-64,-49][-48,-33][-32,-17][-16,-1]
     * NEther  [0~127]
     * The End [0~255]
     */

    namespace {
        bool load_raw(leveldb::DB *&db, const chunk_key &key, std::string &raw) {
            auto r = db->Get(leveldb::ReadOptions(), key.to_raw(), &raw);
            return r.ok();
        }
    }  // namespace

    bool chunk::valid_in_chunk_pos(int cx, int y, int cz, int dim) {
        if (cx < 0 || cx > 15 || cz < 0 || cz > 15 || dim < 0 || dim > 2) return false;
        int min_h[]{-64, 0, 0};
        int max_h[]{319, 127, 255};
        return y >= min_h[dim] && y <= max_h[dim];
    }

    void chunk::map_y_to_subchunk(int y, int &index, int &offset) {
        index = y < 0 ? (y - 15) / 16 : y / 16;
        offset = y % 16;
        if (offset < 0) offset += 16;
    }

    /**
     *
     * @param cx 区块内x
     * @param y  区块内y(同时也是主世界y)
     * @param cz 区块内z
     * @return
     */
    block_info chunk::get_block(int cx, int y, int cz) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return {};
        }
        return it->second.get_block(cx, offset, cz);
    }

    biome chunk::get_biome(int cx, int y, int cz) { return this->d3d_.get_biome(cx, y, cz); }
    void chunk::load_data(bedrock_level &level) {
        if (this->loaded()) return;
        //        BL_LOGGER("Try load chunk %s", this->pos_.to_string().c_str());
        auto &db = level.db();

        auto [min_index, max_index] = this->pos_.get_subchunk_index_range();
        // load all sub chunks
        for (auto sub_index = min_index; sub_index <= max_index; sub_index++) {
            auto terrain_key = bl::chunk_key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            std::string raw;
            if (load_raw(level.db(), terrain_key, raw)) {
                bl::sub_chunk sb;
                if (!sb.load(raw.data(), raw.size())) {
                    BL_ERROR("Can not parse sub chunk content. %s",
                             terrain_key.to_string().c_str());
                } else {
                    this->sub_chunks_[sub_index] = sb;
                }
            }
        }
        if (this->sub_chunks_.empty()) {
            // 没有key实锤了(应该不会出现没有方块数据但是有群系信息的区块)
            return;
        }

        auto d3d_key = bl::chunk_key{chunk_key::Data3D, this->pos_};
        std::string d3d_raw;
        if (load_raw(level.db(), d3d_key, d3d_raw) &&
            this->d3d_.load(d3d_raw.data(), d3d_raw.size())) {
            this->loaded_ = true;
        } else {
            BL_ERROR("Can not load Data3D data %s", this->pos_.to_string().c_str());
        }
        this->loaded_ = true;
        // TODO: load others (actor data3d block entities.etc)
    }

    int chunk::get_height(int cx, int cz) { return this->d3d_.height(cx, cz); }
    biome chunk::get_top_biome(int cx, int cz) { return this->d3d_.get_top_biome(cx, cz); }
    std::array<std::array<biome, 16>, 16> chunk::get_biome_y(int y) {
        return this->d3d_.get_biome_y(y);
    }

}  // namespace bl
