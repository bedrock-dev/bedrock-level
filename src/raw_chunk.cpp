#include "raw_chunk.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "actor.h"
#include "bedrock_key.h"
#include "bedrock_level.h"
#include "binary_io.h"
#include "chunk_data_position.h"
#include "config.h"
#include "leveldb/iterator.h"
#include "nbt.h"
#include "utils.h"

namespace bl {

    namespace {
        void write_bytes(std::vector<byte_t>& buf, std::string_view s) {
            binary::append_i32_le(buf, static_cast<int32_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        }

        bool parse_chunk_format(std::string_view payload, LevelChunkFormat& out) {
            if (payload.empty()) return false;
            const auto value = static_cast<unsigned char>(payload[0]);
            if (value >= static_cast<unsigned char>(LevelChunkFormat::Count)) return false;
            out = static_cast<LevelChunkFormat>(value);
            return true;
        }
    }  // namespace

    void raw_chunk::clear_terrain() {
        for (auto& kv : this->sub_chunk_data_) kv.second.clear();
    }

    void raw_chunk::clear_entities() {
        this->actor_digest_.clear();
        this->entities_.clear();
    }

    bool raw_chunk::read(bedrock_level& level, chunk_load_policy policy) {
        // Marker keys are always read; any present marker makes the chunk valid.
        bool valid = false;
        for (auto kt : MARKER_KEYS) {
            bl::chunk_key key{kt, this->pos_};
            std::string raw;
            const bool present = level.load_raw(key.to_raw(), raw);
            if (present && (!bl::config::strict_chunk_existence() || !raw.empty())) {
                valid = true;
                parse_chunk_format(raw, this->chunk_format_);
                this->data_[kt] = std::move(raw);
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
                                                   chunk_key::ActorDigestVersion,
                                                   chunk_key::AabbVolumes};
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

        if (has_flag(policy, chunk_load_policy::Jigsaw)) {
            const auto prefix = bl::chunk_key{chunk_key::JigsawStructureBlueprint, this->pos_}.to_raw().substr(0, 13);
            std::unique_ptr<leveldb::Iterator> iterator(level.db()->NewIterator(level.bulk_read_options()));
            for (iterator->Seek(prefix); iterator->Valid(); iterator->Next()) {
                const auto& db_key = iterator->key();
                if (db_key.size() < prefix.size() || std::memcmp(db_key.data(), prefix.data(), prefix.size()) != 0) break;
                const auto parsed = bl::chunk_key::parse(std::string_view(db_key.data(), db_key.size()));
                if (parsed.valid() && parsed.type == chunk_key::JigsawStructureBlueprint) {
                    this->jigsaw_data_[parsed.identifier_hash] = iterator->value().ToString();
                }
            }
        }

        if (has_flag(policy, chunk_load_policy::Terrain)) {
            const auto [min_index, max_index] = bl::config::subchunk_index_range();
            for (int sub_index = min_index; sub_index <= max_index; sub_index++) {
                bl::chunk_key key{chunk_key::SubChunkTerrain, this->pos_, static_cast<int8_t>(sub_index)};
                std::string raw;
                level.load_raw(key.to_raw(), raw);
                this->sub_chunk_data_[static_cast<int8_t>(sub_index)] = std::move(raw);
            }
        }

        if (has_flag(policy, chunk_load_policy::Actor)) {
            bl::actor_digest_key digest_key{this->pos_};
            std::string raw;
            if (level.load_raw(digest_key.to_raw(), raw) && !raw.empty()) {
                this->actor_digest_ = raw;
                bl::actor_digest_list list;
                list.load(raw);
                for (auto& key : list.actor_digests_) {
                    auto actor_key = std::string(storage_key::actor) + key;
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

        for (const auto& [identifier_hash, raw] : this->jigsaw_data_) {
            const bl::chunk_key key{chunk_key::JigsawStructureBlueprint, this->pos_, 0, identifier_hash};
            if (clear) {
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
                batch.Delete(std::string(storage_key::actor) + uid);
            }
            bl::actor_digest_key digest_key{this->pos_};
            batch.Delete(digest_key.to_raw());
        } else {
            bl::actor_digest_key digest_key{this->pos_};
            batch.Put(digest_key.to_raw(), this->actor_digest_);
            for (auto& [uid, raw] : this->entities_) {
                batch.Put(std::string(storage_key::actor) + uid, raw);
            }
        }
        return true;
    }

    std::vector<byte_t> raw_chunk::to_raw() {
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
        if (!jigsaw_data_.empty()) {
            size += 4;
            for (const auto& [identifier_hash, raw] : jigsaw_data_) size += 8 + 4 + raw.size();
        }

        std::vector<byte_t> buf;
        buf.reserve(size);
        buf.insert(buf.end(), {'B', 'C', 'H', 'K'});
        binary::append_i32_le(buf, pos_.x);
        binary::append_i32_le(buf, pos_.z);
        binary::append_i32_le(buf, pos_.dim);

        binary::append_i32_le(buf, static_cast<int32_t>(data_.size()));
        for (auto& [kt, raw] : data_) {
            binary::append_i32_le(buf, static_cast<int32_t>(kt));
            write_bytes(buf, raw);
        }

        binary::append_i32_le(buf, static_cast<int32_t>(sub_chunk_data_.size()));
        for (auto& [index, raw] : sub_chunk_data_) {
            buf.push_back(static_cast<byte_t>(index));
            write_bytes(buf, raw);
        }

        write_bytes(buf, actor_digest_);

        binary::append_i32_le(buf, static_cast<int32_t>(entities_.size()));
        for (auto& [uid, raw] : entities_) {
            write_bytes(buf, uid);
            write_bytes(buf, raw);
        }

        // Optional trailer keeps old BCHK exports byte-identical when no
        // Jigsaw records are present, while preserving the full key suffix
        // for exports that contain them.
        if (!jigsaw_data_.empty()) {
            binary::append_i32_le(buf, static_cast<int32_t>(jigsaw_data_.size()));
            for (const auto& [identifier_hash, raw] : jigsaw_data_) {
                binary::append_u64_le(buf, identifier_hash);
                write_bytes(buf, raw);
            }
        }

        return buf;
    }

    bool raw_chunk::from_raw(const std::vector<byte_t>& data) {
        if (data.size() < 4 || data[0] != 'B' || data[1] != 'C' || data[2] != 'H' || data[3] != 'K') {
            return false;
        }
        binary::reader reader(data.data(), data.size());
        if (!reader.skip(4) || !reader.read_i32_le(pos_.x) || !reader.read_i32_le(pos_.z) || !reader.read_i32_le(pos_.dim)) return false;

        int32_t data_count = 0;
        if (!reader.read_i32_le(data_count) || data_count < 0) return false;
        for (int32_t i = 0; i < data_count; i++) {
            int32_t key_type = 0;
            std::string raw;
            if (!reader.read_i32_le(key_type) || !reader.read_bytes(raw)) return false;
            data_[static_cast<chunk_key::key_type>(key_type)] = std::move(raw);
        }
        // Same priority as read(): the marker coming last in MARKER_KEYS wins.
        for (auto kt : MARKER_KEYS) {
            auto it = data_.find(kt);
            if (it != data_.end()) parse_chunk_format(it->second, chunk_format_);
        }

        int32_t sub_count = 0;
        if (!reader.read_i32_le(sub_count) || sub_count < 0) return false;
        for (int32_t i = 0; i < sub_count; i++) {
            uint8_t raw_index = 0;
            std::string raw;
            if (!reader.read_u8(raw_index) || !reader.read_bytes(raw)) return false;
            sub_chunk_data_[static_cast<int8_t>(raw_index)] = std::move(raw);
        }

        if (!reader.read_bytes(actor_digest_)) return false;

        int32_t entity_count = 0;
        if (!reader.read_i32_le(entity_count) || entity_count < 0) return false;
        for (int32_t i = 0; i < entity_count; i++) {
            std::string uid;
            std::string raw;
            if (!reader.read_bytes(uid) || !reader.read_bytes(raw)) return false;
            entities_[std::move(uid)] = std::move(raw);
        }
        if (reader.remaining() > 0) {
            int32_t jigsaw_count = 0;
            if (!reader.read_i32_le(jigsaw_count) || jigsaw_count < 0) return false;
            for (int32_t i = 0; i < jigsaw_count; ++i) {
                uint64_t identifier_hash = 0;
                std::string raw;
                if (!reader.read_u64_le(identifier_hash) || !reader.read_bytes(raw)) return false;
                jigsaw_data_[identifier_hash] = std::move(raw);
            }
        }
        return true;
    }

    std::string_view raw_chunk::get_normal_key(chunk_key::key_type key) const noexcept {
        auto it = this->data_.find(key);
        if (it != this->data_.end()) return it->second;
        return {};
    }

    std::string_view raw_chunk::get_sub_chunk(int8_t yindex) const noexcept {
        auto it = this->sub_chunk_data_.find(yindex);
        if (it != this->sub_chunk_data_.end()) return it->second;
        return {};
    }

    std::pair<int, int> raw_chunk::get_y_range() const {
        // Skip empty entries; the map keeps indices in ascending order.
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

    void raw_chunk::move_to(const bl::chunk_pos& pos, bl::bedrock_level* level) {
        int dx = (pos.x - this->pos_.x) * 16;
        int dz = (pos.z - this->pos_.z) * 16;
        this->pos_ = pos;
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

        if (auto it = data_.find(chunk_key::PendingTicks); it != data_.end()) {
            auto& data = it->second;
            auto palette = nbt::read_palette_to_end(data.data(), data.size());
            for (auto*& p : palette) offset_pending_ticks_pos(p, dx, dz);
            data.clear();
            for (auto* p : palette) data += p->to_raw();
            for (auto* p : palette) delete p;
        }

        if (auto it = data_.find(chunk_key::HardCodedSpawnAreas); it != data_.end()) {
            auto& data = it->second;
            bl::hardcoded_spawn_area_list list;
            if (list.from_raw(data)) {
                offset_hardcoded_spawn_areas_pos(list, dx, dz);
                data = list.to_raw();
            }
        }

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

        std::map<std::string, std::string> new_entities;
        for (auto& [uid, raw] : entities_) {
            actor ac;
            if (ac.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size())) {
                auto new_uid = level->generate_actor_uid();
                ac.reassign_uid(new_uid);
                ac.offset_pos(static_cast<float>(dx), 0.0f, static_cast<float>(dz));
                new_entities.emplace(ac.storage_key_raw(), ac.root()->to_raw());
            } else {
                LOG_F(ERROR, "load actor (uid len=%llu) failed when reset raw chunk position", static_cast<unsigned long long>(uid.size()));
            }
        }
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

    void raw_chunk::set_entities(const std::vector<bl::actor*> entities) {
        actor_digest_.clear();
        set_normal(chunk_key::Entity, "");
        entities_.clear();
        if (!uses_individual_actor_storage(this->chunk_format_)) {
            std::string chunk_actor_data;
            for (auto* a : entities) {
                if (!a) continue;
                chunk_actor_data += a->root()->to_raw();
            }
            set_normal(chunk_key::Entity, chunk_actor_data);
        } else {
            for (auto* ac : entities) {
                entities_[ac->storage_key_raw()] = ac->root()->to_raw();
                this->actor_digest_ += ac->storage_key_raw();
            }
        }
    }

}  // namespace bl
