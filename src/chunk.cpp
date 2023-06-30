//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"

#include <sstream>
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
        bool load_raw(leveldb::DB *&db, const std::string &raw_key, std::string &raw) {
            auto r = db->Get(leveldb::ReadOptions(), raw_key, &raw);
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
        return it->second->get_block(cx, offset, cz);
    }

    block_info chunk::get_top_block(int cx, int cz) {
        auto height = this->get_height(cx, cz);
        auto [min_y, _] = this->pos_.get_y_range();
        return this->get_block(cx, height + min_y - 1, cz);
    }

    palette::compound_tag *chunk::get_block_raw(int cx, int y, int cz) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return nullptr;
        }
        return it->second->get_block_raw(cx, offset, cz);
    }

    biome chunk::get_biome(int cx, int y, int cz) { return this->d3d_.get_biome(cx, y, cz); }

    bool chunk::load_actor_digest(bedrock_level &level) {
        bl::actor_digest_key key{this->pos_};
        std::string raw;
        // 没啥要解析的，不用管错误
        load_raw(level.db(), key.to_raw(), raw);
        this->actor_digest_list_.load(raw);
        return false;
    }

    bool chunk::load_subchunks_(bedrock_level &level) {
        auto [min_index, max_index] = this->pos_.get_subchunk_index_range();
        for (auto sub_index = min_index; sub_index <= max_index; sub_index++) {
            // load all sub chunks
            auto terrain_key = bl::chunk_key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            std::string raw;
            if (load_raw(level.db(), terrain_key.to_raw(), raw)) {
                auto *sb = new bl::sub_chunk();
                if (!sb->load(raw.data(), raw.size())) {
                    return false;
                }

                this->sub_chunks_[sub_index] = sb;
            }
        }

        return true;
    }
    bool chunk::load_d3d(bedrock_level &level) {
        auto d3d_key = bl::chunk_key{chunk_key::Data3D, this->pos_};
        std::string d3d_raw;
        if (load_raw(level.db(), d3d_key.to_raw(), d3d_raw) &&
            this->d3d_.load(d3d_raw.data(), d3d_raw.size())) {
            return true;
        } else {
            //            BL_ERROR("Can not load Data3D data %s", this->pos_.to_string().c_str());
            return false;
        }
    }
    bool chunk::load_pending_ticks(bedrock_level &level) {
        auto pt_key = bl::chunk_key{chunk_key::PendingTicks, this->pos_};
        std::string block_entity_raw;
        if (load_raw(level.db(), pt_key.to_raw(), block_entity_raw) && !block_entity_raw.empty()) {
            this->pending_ticks_ =
                palette::read_palette_to_end(block_entity_raw.data(), block_entity_raw.size());
            //
        }
        return true;
    }

    bool chunk::load_block_entities(bedrock_level &level) {
        auto be_key = bl::chunk_key{chunk_key::BlockEntity, this->pos_};
        std::string block_entity_raw;
        if (load_raw(level.db(), be_key.to_raw(), block_entity_raw) && !block_entity_raw.empty()) {
            this->block_entities_ =
                palette::read_palette_to_end(block_entity_raw.data(), block_entity_raw.size());
            //
        }

        return true;
    }

    bool chunk::load_data(bedrock_level &level) {
        if (this->loaded()) return true;
        auto l1 = this->load_subchunks_(level);
        auto l2 = this->load_d3d(level);
        this->load_actor_digest(level);
        this->load_block_entities(level);
        this->load_pending_ticks(level);
        this->loaded_ = l1 && l2;
        return this->loaded_;
    }

    int chunk::get_height(int cx, int cz) { return this->d3d_.height(cx, cz); }
    biome chunk::get_top_biome(int cx, int cz) { return this->d3d_.get_top_biome(cx, cz); }

    std::array<std::array<biome, 16>, 16> chunk::get_biome_y(int y) {
        return this->d3d_.get_biome_y(y);
    }
    bl::chunk_pos chunk::get_pos() const { return this->pos_; }
    chunk::~chunk() {
        for (auto &sub : this->sub_chunks_) {
            delete sub.second;
        }
    }
    bl::color chunk::get_block_color(int cx, int y, int cz) {
        auto *raw = this->get_block_raw(cx, y, cz);
        if (!raw) return {};
        auto *copy = dynamic_cast<palette::compound_tag *>(raw->copy());
        if (!copy) return {};
        copy->remove("version");
        std::stringstream ss;
        copy->write(ss, 0);
        delete copy;
        return get_block_color_from_snbt(ss.str());
    }
    bl::color chunk::get_top_block_color(int cx, int cz) {
        auto height = this->get_height(cx, cz);
        auto [min_y, _] = this->pos_.get_y_range();
        return this->get_block_color(cx, height + min_y - 1, cz);
    }

}  // namespace bl
