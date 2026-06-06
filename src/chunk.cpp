//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"

#include <string>
#include <utility>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "color.h"
#include "include/utils.h"
#include "leveldb/write_batch.h"
#include "utils.h"

namespace bl {

    /**
     * Overworld [-64 ~-1]+[0~319]
     * [-64,-49][-48,-33][-32,-17][-16,-1]
     * NEther  [0~127]
     * The End [0~255]
     */

    namespace {
        bool contains_key(leveldb::DB *&db, const std::string &raw_key) {
            std::string raw;
            auto r = db->Get(leveldb::ReadOptions(), raw_key, &raw);
            return r.ok();
        }

        void write_i32(std::vector<byte_t> &buf, int32_t v) {
            buf.push_back(static_cast<byte_t>(v & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 8) & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 16) & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 24) & 0xff));
        }

        int32_t read_i32(const byte_t *&p) {
            int32_t v = static_cast<int32_t>(static_cast<uint8_t>(p[0])) | (static_cast<int32_t>(static_cast<uint8_t>(p[1])) << 8) |
                        (static_cast<int32_t>(static_cast<uint8_t>(p[2])) << 16) | (static_cast<int32_t>(static_cast<uint8_t>(p[3])) << 24);
            p += 4;
            return v;
        }

        void write_bytes(std::vector<byte_t> &buf, const std::string &s) {
            write_i32(buf, static_cast<int32_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        }

        std::string read_bytes(const byte_t *&p) {
            int32_t size = read_i32(p);
            std::string s(p, p + size);
            p += size;
            return s;
        }
    }  // namespace

    // do not remove entities
    void raw_chunk::clear_terrain() {
        for (auto &kv : this->sub_chunk_data_) kv.second.clear();
    }

    bool raw_chunk::read(bedrock_level &level) {
        static const chunk_key::key_type keys[] = {
            chunk_key::Data3D,
            chunk_key::VersionNew,
            chunk_key::VersionOld,
            chunk_key::Data2D,
            chunk_key::Data2DLegacy,
            chunk_key::BlockEntity,
            chunk_key::Entity,
            chunk_key::PendingTicks,
            chunk_key::BlockExtraData,
            chunk_key::BiomeState,
            chunk_key::FinalizedState,
            chunk_key::ConversionData,
            chunk_key::BorderBlocks,
            chunk_key::HardCodedSpawnAreas,
            chunk_key::RandomTicks,
            chunk_key::Checksums,
            chunk_key::GenerationSeed,
            chunk_key::GeneratedPreCavesAndCliffsBlending,
            chunk_key::BlendingBiomeHeight,
            chunk_key::MetaDataHash,
            chunk_key::BlendingData,
            chunk_key::ActorDigestVersion,
        };

        for (auto kt : keys) {
            bl::chunk_key key{kt, this->pos_};
            std::string raw;
            if (level.load_raw(key.to_raw(), raw) && !raw.empty()) {
                this->data_[kt] = std::move(raw);
            }
        }

        // read sub chunks
        auto [min_index, max_index] = this->pos_.get_subchunk_index_range(version());
        std::string raw;
        for (auto sub_index = min_index; sub_index <= max_index; sub_index++) {
            bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, sub_index};
            level.load_raw(key.to_raw(), raw);
            this->sub_chunk_data_[sub_index] = std::move(raw);
        }

        // read actor digest and entities
        {
            bl::actor_digest_key digest_key{this->pos_};
            std::string raw;
            if (level.load_raw(digest_key.to_raw(), raw) && !raw.empty()) {
                this->actor_digest_list_ = raw;
                bl::actor_digest_list list;
                list.load(raw);
                for (auto &uid : list.actor_digests_) {
                    auto actor_key = "actorprefix" + uid;
                    std::string raw_actor;
                    if (level.load_raw(actor_key, raw_actor) && !raw_actor.empty()) {
                        this->entities_[uid] = std::move(raw_actor);
                    }
                }
            }
        }
        this->loaded_ = !this->data_.empty();
        return this->loaded_;
    }

    bool raw_chunk::write(leveldb::WriteBatch &batch, bool clear) {
        if (!this->loaded_) return false;

        for (auto &[kt, raw] : this->data_) {
            bl::chunk_key key{kt, this->pos_};
            if (clear) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        for (auto &[index, raw] : this->sub_chunk_data_) {
            bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, index};
            if (clear) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        if (clear) {
            for (auto &[uid, raw] : this->entities_) {
                batch.Delete("actorprefix" + uid);
            }
            bl::actor_digest_key digest_key{this->pos_};
            batch.Delete(digest_key.to_raw());
        } else {
            if (!this->actor_digest_list_.empty()) {
                bl::actor_digest_key digest_key{this->pos_};
                batch.Put(digest_key.to_raw(), this->actor_digest_list_);
            }
            for (auto &[uid, raw] : this->entities_) {
                batch.Put("actorprefix" + uid, raw);
            }
        }
        return true;
    }

    std::vector<byte_t> raw_chunk::to_raw() {
        std::vector<byte_t> buf;
        // magic
        buf.insert(buf.end(), {'B', 'C', 'H', 'K'});
        // pos
        write_i32(buf, pos_.x);
        write_i32(buf, pos_.z);
        write_i32(buf, pos_.dim);

        // normal keys
        write_i32(buf, static_cast<int32_t>(data_.size()));
        for (auto &[kt, raw] : data_) {
            write_i32(buf, static_cast<int32_t>(kt));
            write_bytes(buf, raw);
        }

        // sub chunks
        write_i32(buf, static_cast<int32_t>(sub_chunk_data_.size()));
        for (auto &[index, raw] : sub_chunk_data_) {
            buf.push_back(static_cast<byte_t>(index));
            write_bytes(buf, raw);
        }

        // actor digest
        write_bytes(buf, actor_digest_list_);

        // entities
        write_i32(buf, static_cast<int32_t>(entities_.size()));
        for (auto &[uid, raw] : entities_) {
            write_bytes(buf, uid);
            write_bytes(buf, raw);
        }

        return buf;
    }

    bool raw_chunk::from_raw(const std::vector<byte_t> &data) {
        const byte_t *p = data.data();
        const byte_t *end = data.data() + data.size();

        // magic
        if (static_cast<size_t>(end - p) < 4 || p[0] != 'B' || p[1] != 'C' || p[2] != 'H' || p[3] != 'K') {
            return false;
        }
        p += 4;

        // pos
        pos_.x = read_i32(p);
        pos_.z = read_i32(p);
        pos_.dim = read_i32(p);

        // normal keys
        int32_t data_count = read_i32(p);
        for (int32_t i = 0; i < data_count; i++) {
            auto kt = static_cast<chunk_key::key_type>(read_i32(p));
            data_[kt] = read_bytes(p);
        }

        // sub chunks
        int32_t sub_count = read_i32(p);
        for (int32_t i = 0; i < sub_count; i++) {
            int8_t index = static_cast<int8_t>(*p++);
            sub_chunk_data_[index] = read_bytes(p);
        }

        // actor digest
        actor_digest_list_ = read_bytes(p);

        // entities
        int32_t entity_count = read_i32(p);
        for (int32_t i = 0; i < entity_count; i++) {
            std::string uid = read_bytes(p);
            entities_[std::move(uid)] = read_bytes(p);
        }

        this->loaded_ = true;
        return true;
    }

    std::string raw_chunk::get_normal_key(chunk_key::key_type key) const {
        auto it = this->data_.find(key);
        if (it != this->data_.end()) return it->second;
        return {};
    }

    std::string raw_chunk::get_sub_chunk(int8_t yindex) const {
        auto it = this->sub_chunk_data_.find(yindex);
        if (it != this->sub_chunk_data_.end()) return it->second;
        return {};
    }

    bool chunk::valid_in_chunk_pos(int cx, int y, int cz, int dim) {
        if (cx < 0 || cx > 15 || cz < 0 || cz > 15 || dim < 0 || dim > 2) return false;
        static constexpr int min_h[]{-64, 0, 0};
        static constexpr int max_h[]{319, 127, 255};
        return y >= min_h[dim] && y <= max_h[dim];
    }

    void chunk::map_y_to_subchunk(int y, int &index, int &offset) {
        index = y < 0 ? (y - 15) / 16 : y / 16;
        offset = y % 16;
        if (offset < 0) offset += 16;
    }

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

    bool chunk::load_subchunks(const bl::raw_chunk &rc) {
        for (auto &[sub_index, raw] : rc.get_sub_chunks()) {
            if (raw.empty()) continue;
            auto *sb = new bl::sub_chunk();
            sb->set_y_index(sub_index);
            if (!sb->load(raw.data(), raw.size())) {
                BL_ERROR("Can not load sub chunk %s / %d", pos_.to_string().c_str(), sub_index);
                delete sb;
                continue;
            }
            this->sub_chunks_[sub_index] = sb;
        }
        if (sub_chunks_.empty()) {
            // BL_ERROR("Can not load terrain data of chunk %s", pos_.to_string().c_str());
        } else {
            this->version = this->sub_chunks_.begin()->second->version() == 9 ? New : Old;
        }
        return true;
    }

    bool chunk::load_biomes(const bl::raw_chunk &rc) {
        this->d3d_.set_chunk_pos(this->pos_);
        this->d3d_.set_version(this->version);
        if (this->version == New) {
            auto raw = rc.get_normal_key(chunk_key::Data3D);
            return !raw.empty() && this->d3d_.load_from_d3d(raw.data(), raw.size());
        } else {
            auto raw = rc.get_normal_key(chunk_key::Data2D);
            return !raw.empty() && this->d3d_.load_from_d2d(raw.data(), raw.size());
        }
    }

    bool chunk::load_pending_ticks(const bl::raw_chunk &rc) {
        auto raw = rc.get_normal_key(chunk_key::PendingTicks);
        if (!raw.empty()) {
            this->pending_ticks_ = palette::read_palette_to_end(raw.data(), raw.size());
        }
        return true;
    }
    void chunk::load_entities(const bl::raw_chunk &rc) {
        // try read old version actors
        auto raw = rc.get_normal_key(chunk_key::Entity);
        if (!raw.empty()) {
            auto actors = palette::read_palette_to_end(raw.data(), raw.size());
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
        // new version actors from raw_chunk
        const auto &digest_raw = rc.get_actor_digest();
        if (!digest_raw.empty()) {
            bl::actor_digest_list list;
            list.load(digest_raw);
            const auto &rc_entities = rc.get_entities();
            for (auto &uid : list.actor_digests_) {
                auto it = rc_entities.find(uid);
                if (it != rc_entities.end() && !it->second.empty()) {
                    auto ac = new actor;
                    if (!ac->load(it->second.data(), it->second.size())) {
                        delete ac;
                    } else {
                        this->entities_.push_back(ac);
                    }
                }
            }
        }
    }

    void chunk::load_hsa(const bl::raw_chunk &rc) {
        auto raw = rc.get_normal_key(chunk_key::HardCodedSpawnAreas);
        if (raw.empty() || raw.size() < 4) return;
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
            if (type == SwampHut || type == OceanMonument || type == NetherFortress || type == PillagerOutpost) {
                area.type = static_cast<HSAType>(type);
            }
            this->HSAs_.push_back(area);
        }
    }
    bool chunk::load_block_entities(const bl::raw_chunk &rc) {
        auto raw = rc.get_normal_key(chunk_key::BlockEntity);
        if (!raw.empty()) {
            this->block_entities_ = palette::read_palette_to_end(raw.data(), raw.size());
        }
        return true;
    }

    bool chunk::load_data(bedrock_level &level, bool fast_load) {
        if (this->loaded()) return true;
        bl::raw_chunk rc(this->pos_);
        if (!rc.read(level)) return false;
        return this->load_from_raw_chunk(rc);
    }

    bool chunk::load_from_raw_chunk(const bl::raw_chunk &rc) {
        this->load_subchunks(rc);
        if (this->sub_chunks_.empty()) return false;
        this->load_biomes(rc);
        this->load_entities(rc);
        this->load_block_entities(rc);
        this->load_pending_ticks(rc);
        this->load_hsa(rc);
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
        auto b = this->get_block_fast(cx, y, cz);
        return get_block_by_name_tag(b.name);
    }
}  // namespace bl
