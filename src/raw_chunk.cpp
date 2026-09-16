//
// Created by xhy on 2023/3/30.
//

#include "raw_chunk.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "actor.h"
#include "bedrock_key.h"
#include "bedrock_level.h"
#include "chunk_data_position.h"
#include "config.h"
#include "data_3d.h"
#include "nbt.h"
#include "utils.h"

namespace bl {

    namespace {
        void write_i32(std::vector<byte_t>& buf, int32_t v) {
            buf.push_back(static_cast<byte_t>(v & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 8) & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 16) & 0xff));
            buf.push_back(static_cast<byte_t>((v >> 24) & 0xff));
        }

        int32_t read_i32(const byte_t*& p) {
            int32_t v = static_cast<int32_t>(static_cast<uint8_t>(p[0])) | (static_cast<int32_t>(static_cast<uint8_t>(p[1])) << 8) |
                        (static_cast<int32_t>(static_cast<uint8_t>(p[2])) << 16) | (static_cast<int32_t>(static_cast<uint8_t>(p[3])) << 24);
            p += 4;
            return v;
        }

        void write_bytes(std::vector<byte_t>& buf, const std::string& s) {
            write_i32(buf, static_cast<int32_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        }

        std::string read_bytes(const byte_t*& p) {
            int32_t size = read_i32(p);
            std::string s(p, p + size);
            p += size;
            return s;
        }
    }  // namespace

    // do not remove entities
    void raw_chunk::clear_terrain() {
        for (auto& kv : this->sub_chunk_data_) kv.second.clear();
    }

    void raw_chunk::clear_entities() {
        this->actor_digest_.clear();
        this->entities_.clear();
    }

    bool raw_chunk::read(bedrock_level& level, chunk_load_policy policy) {
        // Version/terrain marker keys are always read regardless of policy.
        // The chunk is valid if any of them exists.
        bool valid = false;
        for (auto kt : MARKER_KEYS) {
            bl::chunk_key key{kt, this->pos_};
            std::string raw;
            const bool present = level.load_raw(key.to_raw(), raw);
            if (present && (!bl::config::strict_chunk_existence() || !raw.empty())) {
                valid = true;
                this->data_[kt] = std::move(raw);
                if (kt == chunk_key::VersionNew) this->version_ = ChunkVersion::New;
            }
        }
        if (!valid) return false;
        static const chunk_key::key_type keys[] = {chunk_key::Data3D,
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
                                                   chunk_key::ActorDigestVersion};
        auto flagFor = [](chunk_key::key_type kt) -> chunk_load_policy {
            switch (kt) {
                case chunk_key::Data3D:
                case chunk_key::Data2D:
                case chunk_key::Data2DLegacy:
                    return chunk_load_policy::Terrain;
                case chunk_key::BlockEntity:
                    return chunk_load_policy::BlockActor;
                case chunk_key::Entity:
                    return chunk_load_policy::Actor;
                case chunk_key::PendingTicks:
                    return chunk_load_policy::PendingTick;
                default:
                    return chunk_load_policy::Others;
            }
        };
        for (auto kt : keys) {
            if (!has_flag(policy, flagFor(kt))) continue;
            bl::chunk_key key{kt, this->pos_};
            std::string raw;
            if (level.load_raw(key.to_raw(), raw) && !raw.empty()) {
                this->data_[kt] = std::move(raw);
            }
        }

        // read sub chunks
        if (has_flag(policy, chunk_load_policy::Terrain)) {
            // The window is configurable: dimensions whose terrain lies outside the default
            // -4..19 (y -64..319) need it widened or their sub-chunks never load.
            const auto [min_index, max_index] = bl::config::subchunk_index_range();
            for (int sub_index = min_index; sub_index <= max_index; sub_index++) {
                bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, static_cast<int8_t>(sub_index)};
                std::string raw;
                level.load_raw(key.to_raw(), raw);
                this->sub_chunk_data_[static_cast<int8_t>(sub_index)] = std::move(raw);
            }
        }

        // read actor digest and entities
        if (has_flag(policy, chunk_load_policy::Actor)) {
            bl::actor_digest_key digest_key{this->pos_};
            std::string raw;
            if (level.load_raw(digest_key.to_raw(), raw) && !raw.empty()) {
                this->actor_digest_ = raw;
                bl::actor_digest_list list;
                list.load(raw);
                for (auto& key : list.actor_digests_) {
                    auto actor_key = "actorprefix" + key;
                    std::string raw_actor;
                    if (level.load_raw(actor_key, raw_actor) && !raw_actor.empty()) {
                        this->entities_[key] = std::move(raw_actor);
                    } else {
                        if (bl::config::log_mismatched_actor()) LOG_F(ERROR, "actor data of key '%s' is empty", actor_key.c_str());
                    }
                }
            }
        }
        return true;
    }

    bool raw_chunk::write(leveldb::WriteBatch& batch, bool clear) {
        // nothing to write unless some normal key holds data
        bool has_data = false;
        for (const auto& [kt, raw] : this->data_) {
            if (!raw.empty()) {
                has_data = true;
                break;
            }
        }
        if (!has_data) return false;

        for (auto& [kt, raw] : this->data_) {
            bl::chunk_key key{kt, this->pos_};
            if (clear || raw.empty()) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        for (auto& [index, raw] : this->sub_chunk_data_) {
            bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, index};
            if (clear || raw.empty()) {
                batch.Delete(key.to_raw());
            } else {
                batch.Put(key.to_raw(), raw);
            }
        }

        if (clear || actor_digest_.empty()) {
            for (auto& [uid, raw] : this->entities_) {
                batch.Delete("actorprefix" + uid);
            }
            bl::actor_digest_key digest_key{this->pos_};
            batch.Delete(digest_key.to_raw());
        } else {
            bl::actor_digest_key digest_key{this->pos_};
            batch.Put(digest_key.to_raw(), this->actor_digest_);
            for (auto& [uid, raw] : this->entities_) {
                batch.Put("actorprefix" + uid, raw);
            }
        }
        return true;
    }

    std::vector<byte_t> raw_chunk::to_raw() {
        // pre-compute exact size so the buffer is only reallocated once
        size_t size = 4 /*magic*/ + 3 * 4 /*pos*/ + 4 /*data count*/;
        for (auto& [kt, raw] : data_) {
            size += 4 /*kt*/ + 4 /*len*/ + raw.size();
        }
        size += 4 /*sub count*/;
        for (auto& [index, raw] : sub_chunk_data_) {
            size += 1 /*index*/ + 4 /*len*/ + raw.size();
        }
        size += 4 + actor_digest_.size();  // len + digest
        size += 4 /*entity count*/;
        for (auto& [uid, raw] : entities_) {
            size += 4 + uid.size() + 4 + raw.size();
        }

        std::vector<byte_t> buf;
        buf.reserve(size);
        // magic
        buf.insert(buf.end(), {'B', 'C', 'H', 'K'});
        // pos
        write_i32(buf, pos_.x);
        write_i32(buf, pos_.z);
        write_i32(buf, pos_.dim);

        // normal keys
        write_i32(buf, static_cast<int32_t>(data_.size()));
        for (auto& [kt, raw] : data_) {
            write_i32(buf, static_cast<int32_t>(kt));
            write_bytes(buf, raw);
        }

        // sub chunks
        write_i32(buf, static_cast<int32_t>(sub_chunk_data_.size()));
        for (auto& [index, raw] : sub_chunk_data_) {
            buf.push_back(static_cast<byte_t>(index));
            write_bytes(buf, raw);
        }

        // actor digest
        write_bytes(buf, actor_digest_);

        // entities
        write_i32(buf, static_cast<int32_t>(entities_.size()));
        for (auto& [uid, raw] : entities_) {
            write_bytes(buf, uid);
            write_bytes(buf, raw);
        }

        return buf;
    }

    bool raw_chunk::from_raw(const std::vector<byte_t>& data) {
        const byte_t* p = data.data();
        const byte_t* end = data.data() + data.size();

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
        version_ = data_.find(chunk_key::VersionNew) != data_.end() ? ChunkVersion::New : ChunkVersion::Old;

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

    std::pair<int, int> raw_chunk::get_y_range() const {
        // Keys are inserted for every index in the read window, absent ones included, so skip
        // the empty payloads -- otherwise the range would cover sub-chunks that hold nothing.
        // sub_chunk_data_ is a map, so iteration already goes in ascending index order.
        bool found = false;
        int first = 0;
        int last = 0;
        for (const auto& [index, raw] : this->sub_chunk_data_) {
            if (raw.empty()) continue;
            if (!found) {
                first = index;
                found = true;
            }
            last = index;
        }
        if (!found) return {0, -1};
        return {first * 16, last * 16 + 15};
    }

    void raw_chunk::set_pos(const bl::chunk_pos& pos, bl::bedrock_level* level) {
        int dx = (pos.x - this->pos_.x) * 16;
        int dz = (pos.z - this->pos_.z) * 16;
        this->pos_ = pos;
        // block entities
        if (auto it = data_.find(chunk_key::BlockEntity); it != data_.end()) {
            auto& data = it->second;
            auto palette = nbt::read_palette_to_end(data.data(), data.size());
            for (auto*& p : palette) {
                block_pos world_pos;
                if (!read_block_entity_pos(p, world_pos)) continue;
                set_block_entity_pos(p, block_pos{world_pos.x + dx, world_pos.y, world_pos.z + dz});
            }
            data.clear();
            for (auto* p : palette) data += p->to_raw();
            for (auto* p : palette) delete p;
        }

        // pending ticks
        if (auto it = data_.find(chunk_key::PendingTicks); it != data_.end()) {
            auto& data = it->second;
            auto palette = nbt::read_palette_to_end(data.data(), data.size());
            for (auto*& p : palette) offset_pending_ticks_pos(p, dx, dz);
            data.clear();
            for (auto* p : palette) data += p->to_raw();
            for (auto* p : palette) delete p;
        }

        // hardcoded spawn areas
        if (auto it = data_.find(chunk_key::HardCodedSpawnAreas); it != data_.end()) {
            auto& data = it->second;
            bl::hardcoded_spawn_area_list list;
            if (list.from_raw(data)) {
                offset_hardcoded_spawn_areas_pos(list, dx, dz);
                data = list.to_raw();
            }
        }

        // entities (old version: concatenated in Entity key)
        if (auto it = data_.find(chunk_key::Entity); it != data_.end()) {
            auto& data = it->second;
            auto palette = nbt::read_palette_to_end(data.data(), data.size());
            data.clear();
            for (auto* p : palette) {
                if (!p) continue;
                actor ac;
                if (ac.load_from_nbt(p)) {
                    auto uid = level->generate_actor_uid();
                    ac.reassign_uid(uid);
                    ac.offset_pos(static_cast<float>(dx), 0.0f, static_cast<float>(dz));
                    data += ac.root()->to_raw();
                } else {
                    data += p->to_raw();
                }
                delete p;
            }
        }

        // entities (new version: actorprefix+uid)
        std::map<std::string, std::string> new_entities;
        for (auto& [uid, raw] : entities_) {
            actor ac;
            if (ac.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size())) {
                auto new_uid = level->generate_actor_uid();
                ac.reassign_uid(new_uid);
                ac.offset_pos(static_cast<float>(dx), 0.0f, static_cast<float>(dz));
                new_entities.emplace(ac.storage_key_raw(), ac.root()->to_raw());
            } else {
                LOG_F(ERROR, "load actor (uid len=%zu) failed when reset raw chunk position", uid.size());
            }
        }
        // rebuild actor digest and nbts from updated entities_
        entities_ = std::move(new_entities);
        actor_digest_.clear();
        for (auto& [uid, raw] : entities_) {
            actor_digest_ += uid;
        }
    }

    void raw_chunk::set_block_entities(const std::vector<nbt::compound_tag*>& entities) {
        std::string payload;
        for (const auto* entity : entities) {
            if (entity) payload += entity->to_raw();
        }
        set_normal(chunk_key::BlockEntity, payload);
    }

    void raw_chunk::set_entities(const std::vector<bl::actor*> entities, ChunkVersion version) {
        actor_digest_.clear();
        set_normal(chunk_key::Entity, "");
        entities_.clear();
        // set actor by version different chunk version
        if (version == ChunkVersion::Old) {
            std::string chunk_actor_data;
            // create palette
            for (auto* a : entities) {
                if (!a) continue;
                chunk_actor_data += a->root()->to_raw();
            }
            set_normal(chunk_key::Entity, chunk_actor_data);
        } else {
            std::string digest;
            for (auto* ac : entities) {
                entities_[ac->storage_key_raw()] = ac->root()->to_raw();
                this->actor_digest_ += ac->storage_key_raw();
            }
        }
    }

    void raw_chunk::set_biome(biome biome) {
        biome3d d3d;
        d3d.set_chunk_pos(pos_);
        auto raw = get_normal_key(chunk_key::Data3D);
        const bool has3d = !raw.empty();
        if (!has3d) raw = get_normal_key(chunk_key::Data2D);
        if (raw.empty()) return;
        const bool ok = has3d ? d3d.load_from_d3d(reinterpret_cast<const byte_t*>(raw.data()), raw.size())
                              : d3d.load_from_d2d(reinterpret_cast<const byte_t*>(raw.data()), raw.size());
        if (!ok) return;
        d3d.set_all(biome);
        // write back to the key the data came from
        data_[has3d ? chunk_key::Data3D : chunk_key::Data2D] = d3d.to_raw();
    }

    void raw_chunk::set_biome_data(const std::string& payload, bool use_3d) {
        const auto key = use_3d ? chunk_key::Data3D : chunk_key::Data2D;
        if (data_.find(key) == data_.end()) return;
        data_[key] = payload;
    }

}  // namespace bl
