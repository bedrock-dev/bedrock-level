//
// Created by xhy on 2023/3/30.
//

#include "chunk.h"

#include <float.h>

#include <cstddef>
#include <string>
#include <utility>

#include "actor.h"
#include "bedrock_key.h"
#include "bedrock_level.h"
#include "color.h"
#include "include/utils.h"
#include "leveldb/write_batch.h"
#include "palette.h"
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

    void raw_chunk::clear_entities() {
        this->actor_digest_.clear();
        this->entities_.clear();
    }

    bool raw_chunk::read(bedrock_level &level) {
        static const chunk_key::key_type keys[] = {chunk_key::Data3D,
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
                                                   chunk_key::VersionOld};
        size_t read = 0;
        for (auto kt : keys) {
            bl::chunk_key key{kt, this->pos_};
            std::string raw;
            if (level.load_raw(key.to_raw(), raw) && !raw.empty()) {
                read += raw.size();
                this->data_[kt] = std::move(raw);
            }
        }
        if (read == 0) return false;

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
                this->actor_digest_ = raw;
                bl::actor_digest_list list;
                list.load(raw);
                for (auto &key : list.actor_digests_) {
                    auto actor_key = "actorprefix" + key;
                    std::string raw_actor;
                    if (level.load_raw(actor_key, raw_actor) && !raw_actor.empty()) {
                        this->entities_[key] = std::move(raw_actor);
                    } else {
                        BL_ERROR("actor key '%s' is empty", actor_key.c_str());
                    }
                }
            }
        }
        return true;
    }

    bool raw_chunk::write(leveldb::WriteBatch &batch, bool clear) {
        if (!loaded()) return false;

        for (auto &[kt, raw] : this->data_) {
            bl::chunk_key key{kt, this->pos_};
            if (clear || raw.empty()) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        for (auto &[index, raw] : this->sub_chunk_data_) {
            bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, index};
            if (clear || raw.empty()) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        if (clear || actor_digest_.empty()) {
            for (auto &[uid, raw] : this->entities_) {
                batch.Delete("actorprefix" + uid);
            }
            bl::actor_digest_key digest_key{this->pos_};
            batch.Delete(digest_key.to_raw());
        } else {
            bl::actor_digest_key digest_key{this->pos_};
            batch.Put(digest_key.to_raw(), this->actor_digest_);
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
        write_bytes(buf, actor_digest_);

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
        actor_digest_ = read_bytes(p);

        // entities
        int32_t entity_count = read_i32(p);
        for (int32_t i = 0; i < entity_count; i++) {
            std::string uid = read_bytes(p);
            entities_[std::move(uid)] = read_bytes(p);
        }
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

    void raw_chunk::set_pos(const bl::chunk_pos &pos, bl::bedrock_level *level) {
        int dx = (pos.x - this->pos_.x) * 16;
        int dz = (pos.z - this->pos_.z) * 16;
        this->pos_ = pos;
        // block entities
        if (auto it = data_.find(chunk_key::BlockEntity); it != data_.end()) {
            auto &data = it->second;
            auto palette = palette::read_palette_to_end(data.data(), data.size());
            for (auto *p : palette) {
                if (!p) continue;
                auto xtag = dynamic_cast<palette::int_tag *>(p->get("x"));
                auto ztag = dynamic_cast<palette::int_tag *>(p->get("z"));
                if (xtag) xtag->value += dx;
                if (ztag) ztag->value += dz;
            }
            data.clear();
            for (auto *p : palette) data += p->to_raw();
            for (auto *p : palette) delete p;
        }

        // pending ticks
        if (auto it = data_.find(chunk_key::PendingTicks); it != data_.end()) {
            auto &data = it->second;
            auto palette = palette::read_palette_to_end(data.data(), data.size());
            for (auto *p : palette) {
                if (!p) continue;
                auto *tickList = dynamic_cast<bl::palette::list_tag *>(p->get("tickList"));
                if (!tickList) continue;
                for (auto *item : tickList->value) {
                    auto *pt = dynamic_cast<palette::compound_tag *>(item);
                    if (!pt) continue;
                    auto *xtag = dynamic_cast<palette::int_tag *>(pt->get("x"));
                    auto *ztag = dynamic_cast<palette::int_tag *>(pt->get("z"));
                    if (xtag) xtag->value += dx;
                    if (ztag) ztag->value += dz;
                }
            }
            data.clear();
            for (auto *p : palette) data += p->to_raw();
            for (auto *p : palette) delete p;
        }

        // entities (old version: concatenated in Entity key)
        if (auto it = data_.find(chunk_key::Entity); it != data_.end()) {
            auto &data = it->second;
            auto palette = palette::read_palette_to_end(data.data(), data.size());
            data.clear();
            for (auto *p : palette) {
                if (!p) continue;
                actor ac;
                if (ac.load_from_nbt(p)) {
                    auto uid = level->generate_actor_uid();
                    ac.reassign_uid(uid);
                    ac.offset_pos(static_cast<float>(dx), static_cast<float>(dz));
                    data += ac.root()->to_raw();
                } else {
                    data += p->to_raw();
                }
                delete p;
            }
        }

        // entities (new version: actorprefix+uid)
        std::map<std::string, std::string> new_entities;
        for (auto &[uid, raw] : entities_) {
            actor ac;
            if (ac.load(reinterpret_cast<const byte_t *>(raw.data()), raw.size())) {
                auto new_uid = level->generate_actor_uid();
                ac.reassign_uid(new_uid);
                ac.offset_pos(static_cast<float>(dx), static_cast<float>(dz));
                new_entities.emplace(ac.storage_key_raw(), ac.root()->to_raw());
            } else {
                BL_ERROR("load actor (uid len=%zu) failed when reset raw chunk position", uid.size());
            }
        }
        // rebuild actor digest and nbts from updated entities_
        entities_ = std::move(new_entities);
        actor_digest_.clear();
        for (auto &[uid, raw] : entities_) {
            actor_digest_ += uid;
        }
    }

    void raw_chunk::set_entities(const std::vector<bl::actor *> entities) {
        actor_digest_.clear();
        set_normal(chunk_key::Entity, "");
        entities_.clear();
        // set actor by version different chunk version
        if (version() == ChunkVersion::Old) {
            std::string chunk_actor_data;
            // create palette
            for (auto *a : entities) {
                if (!a) continue;
                chunk_actor_data += a->root()->to_raw();
            }
            set_normal(chunk_key::Entity, chunk_actor_data);
        } else {
            std::string digest;
            for (auto *ac : entities) {
                entities_[ac->storage_key_raw()] = ac->root()->to_raw();
                this->actor_digest_ += ac->storage_key_raw();
            }
        }
    }

    void raw_chunk::set_biome(biome biome) {
        auto v = version();
        biome3d d3d;
        d3d.set_chunk_pos(pos_);
        d3d.set_version(v);

        if (v == ChunkVersion::New) {
            auto raw = get_normal_key(chunk_key::Data3D);
            if (raw.empty()) return;
            if (!d3d.load_from_d3d(reinterpret_cast<const byte_t *>(raw.data()), raw.size())) return;
        } else {
            auto raw = get_normal_key(chunk_key::Data2D);
            if (raw.empty()) raw = get_normal_key(chunk_key::Data2DLegacy);
            if (raw.empty()) return;
            if (!d3d.load_from_d2d(reinterpret_cast<const byte_t *>(raw.data()), raw.size())) return;
        }
        d3d.set_all(biome);
        data_[v == ChunkVersion::New ? chunk_key::Data3D : chunk_key::Data2D] = d3d.to_raw();
    }

    // chunk
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
                BL_ERROR("Can not load sub chunk (pos = %s, idx = %d, data size = %zu)", pos_.to_string().c_str(), sub_index, raw.size());
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
                        BL_ERROR("invalid entitiy found");
                    } else {
                        this->entities_.push_back(ac);
                    }
                } else {
                    BL_ERROR("mismatch found between digest and actor palette ");
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

    std::pair<int, int> chunk::get_top_y(int cx, int cz, int max_y) {
        auto [min_y, _] = get_pos().get_y_range(this->version);
        int top_y = min_y - 1;
        int solid_y = min_y - 1;

        for (int y = max_y; y >= min_y; y--) {
            auto b = get_block_fast(cx, y, cz);
            if (b.name == "minecraft:unknown") continue;

            if (top_y < min_y && b.name != "minecraft:air") {
                top_y = y;
            }

            // solid_y is the highest non-air, non-water block at or below top_y
            if (b.name != "minecraft:air" && b.name != "minecraft:water" && solid_y < min_y) {
                solid_y = y;
            }

            if (top_y >= min_y && solid_y >= min_y) break;
        }

        return {top_y, solid_y};
    }
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
}  // namespace bl
