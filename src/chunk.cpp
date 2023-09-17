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

        bool contains_key(leveldb::DB *&db, const std::string &raw_key) {
            std::string raw;
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

    block_info chunk::get_block_fast(int cx, int y, int cz) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return {};
        }
        return it->second->get_block_fast(cx, offset, cz);
    }

    //    block_info chunk::get_top_block(int cx, int cz) {
    //        auto height = this->get_height(cx, cz);
    //        return this->get_block(cx, height - 1, cz);
    //    }
    //
    //    palette::compound_tag *chunk::get_top_block_raw(int cx, int cz) {
    //        auto height = this->get_height(cx, cz);
    //        return this->get_block_raw(cx, height - 1, cz);
    //    }
    //

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

    bool chunk::load_subchunks(bedrock_level &level) {
        // 默认先用new,因为version字段太怪，看不懂版本
        auto [min_index, max_index] = this->pos_.get_subchunk_index_range(ChunkVersion::New);
        for (auto sub_index = min_index; sub_index <= max_index; sub_index++) {
            // load all sub chunks
            auto terrain_key = bl::chunk_key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            std::string raw;
            if (load_raw(level.db(), terrain_key.to_raw(), raw)) {
                auto *sb = new bl::sub_chunk();
                sb->set_y_index(
                    sub_index);  // set default index (no `sub-chunk index`  in version 8 chunks)
                                 // //see
                                 // https://gist.github.com/Tomcc/a96af509e275b1af483b25c543cfbf37?permalink_comment_id=3901255#gistcomment-3901255
                if (!sb->load(raw.data(), raw.size())) {
                    BL_ERROR("Can not load sub chunk %d %d %d %d", pos_.x, pos_.z, pos_.dim,
                             sub_index);
                    delete sb;  // delete error sub chunks
                    continue;
                }
                this->sub_chunks_[sub_index] = sb;
            }
        }

        if (!this->sub_chunks_.empty()) {
            // 根据subchunk格式猜测一个 version，后面可能需要修改
            this->version = this->sub_chunks_.begin()->second->version() == 9 ? New : Old;
        }
        return true;
    }
    bool chunk::load_biomes(bedrock_level &level) {
        this->d3d_.set_chunk_pos(this->pos_);
        this->d3d_.set_version(this->version);
        if (this->version == New) {
            auto d3d_key = bl::chunk_key{chunk_key::Data3D, this->pos_};
            std::string d3d_raw;
            if (load_raw(level.db(), d3d_key.to_raw(), d3d_raw) &&
                this->d3d_.load_from_d3d(d3d_raw.data(), d3d_raw.size())) {
                return true;
            } else {
                return false;
            }

        } else {
            auto d2d_key = bl::chunk_key{chunk_key::Data2D, this->pos_};
            std::string d2d_raw;
            if (load_raw(level.db(), d2d_key.to_raw(), d2d_raw) &&
                this->d3d_.load_from_d2d(d2d_raw.data(), d2d_raw.size())) {
                return true;
            } else {
                return false;
            }
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
    void chunk::load_entities(bedrock_level &level) {
        // try read old version actors
        auto entity_key = bl::chunk_key{chunk_key::Entity, this->pos_};
        std::string block_entity_raw;
        if (load_raw(level.db(), entity_key.to_raw(), block_entity_raw) &&
            !block_entity_raw.empty()) {
            auto actors =
                palette::read_palette_to_end(block_entity_raw.data(), block_entity_raw.size());
            for (auto &a : actors) {
                auto *ac = new actor;
                if (ac->load_from_nbt(a)) {
                    this->entities_.push_back(ac);
                } else {
                    delete ac;
                }
                delete a;
            }
        }
        // new version actor key:
        // 1. read key file  form digest
        // 2. read actor from actor keys [actorprefix+uid]
        bl::actor_digest_key key{this->pos_};
        std::string raw;
        // 没啥要解析的，不用管错误
        if (!load_raw(level.db(), key.to_raw(), raw)) {
            return;
        }

        bl::actor_digest_list list;
        list.load(raw);
        for (auto &uid : list.actor_digests_) {
            auto actor_key = "actorprefix" + uid;
            std::string raw_actor;
            if (load_raw(level.db(), actor_key, raw_actor)) {
                auto ac = new actor;
                if (!ac->load(raw_actor.data(), raw_actor.size())) {
                    delete ac;
                } else {
                    this->entities_.push_back(ac);
                }
            }
        }
    }
    void chunk::load_hsa(bedrock_level &level) {
        auto hsa_key = bl::chunk_key{chunk_key::HardCodedSpawnAreas, this->pos_};
        std::string raw;
        if (!load_raw(level.db(), hsa_key.to_raw(), raw)) return;
        if (raw.size() < 4) return;
        int count = *reinterpret_cast<const int *>(raw.data());
        if (raw.size() != count * 25ul + 4ul) return;

        auto *d = raw.data();
        for (int i = 0; i < count; i++) {
            hardcoded_spawn_area area;
            int offset = i * 25 + 4;
            area.min_pos.x = *reinterpret_cast<const int *>(d + offset);
            area.min_pos.y = *reinterpret_cast<const int *>(d + offset + 4);
            area.min_pos.z = *reinterpret_cast<const int *>(d + offset + 8);
            area.max_pos.x = *reinterpret_cast<const int *>(d + offset + 12);
            area.max_pos.y = *reinterpret_cast<const int *>(d + offset + 16);
            area.max_pos.z = *reinterpret_cast<const int *>(d + offset + 20);
            auto type = d[offset + 24];
            if (type == SwampHut || type == OceanMonument || type == NetherFortress ||
                type == PillagerOutpost) {
                area.type = static_cast<HSAType>(type);
            }
            this->HSAs_.push_back(area);
        }
    }
    bool chunk::load_block_entities(bedrock_level &level) {
        auto be_key = bl::chunk_key{chunk_key::BlockEntity, this->pos_};
        std::string block_entity_raw;
        if (load_raw(level.db(), be_key.to_raw(), block_entity_raw) && !block_entity_raw.empty()) {
            this->block_entities_ =
                palette::read_palette_to_end(block_entity_raw.data(), block_entity_raw.size());
        } else {
        }

        return true;
    }

    bool chunk::load_data(bedrock_level &level, bool fast_load) {
        if (this->loaded()) return true;
        if ((!contains_key(level.db(),
                           bl::chunk_key{chunk_key::VersionOld, this->pos_}.to_raw())) &&
            (!contains_key(level.db(),
                           bl::chunk_key{chunk_key::VersionNew, this->pos_}.to_raw()))) {
            return false;
        }

        this->load_subchunks(level);
        if (this->sub_chunks_.empty()) return false;
        this->load_biomes(level);
        this->load_entities(level);

        if (!fast_load) {
            this->load_block_entities(level);
            this->load_pending_ticks(level);  // 有bug
        }
        this->load_hsa(level);
        this->fast_load_mode_ = fast_load;
        this->loaded_ = true;
        return this->loaded_;
    }

    // 从0开始的数据
    int chunk::get_height(int cx, int cz) { return this->d3d_.height(cx, cz); }
    biome chunk::get_top_biome(int cx, int cz) { return this->d3d_.get_top_biome(cx, cz); }

    std::vector<std::vector<biome>> chunk::get_biome_y(int y) { return this->d3d_.get_biome_y(y); }
    bl::chunk_pos chunk::get_pos() const { return this->pos_; }
    chunk::~chunk() {
        for (auto &sub : this->sub_chunks_) {
            delete sub.second;
        }
        for (auto &p : this->pending_ticks_) delete p;
        for (auto &p : this->block_entities_) delete p;
        for (auto &e : this->entities_) delete e;
    }

    bl::color chunk::get_block_color(int cx, int y, int cz) {
        auto *raw = this->get_block_raw(cx, y, cz);
        if (!raw) return {};
        return get_block_color_from_SNBT(raw->to_raw());
    }

    //    bl::color chunk::get_top_block_color(int cx, int cz) {
    //        auto height = this->get_height(cx, cz);
    //        return this->get_block_color(cx, height - 1, cz);
    //    }

}  // namespace bl
