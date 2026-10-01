#include "chunk.h"

#include <float.h>

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "actor.h"
#include "bedrock_key.h"
#include "bedrock_level.h"
#include "chunk_data_position.h"
#include "config.h"
#include "leveldb/write_batch.h"
#include "nbt.h"
#include "utils.h"

namespace bl {

    namespace {
        bool read_int_tag(const nbt::compound_tag* tag, const char* key, int& out) {
            const auto* value = tag ? tag->get(key) : nullptr;
            const auto* intTag = value ? value->as<const nbt::int_tag*>() : nullptr;
            if (!intTag) return false;
            out = intTag->value;
            return true;
        }

        // Payload of the FinalizedState key: a little-endian int32, 2 meaning the chunk is done.
        const std::string FINALIZED_STATE_PAYLOAD{'\x02', '\0', '\0', '\0'};
    }  // namespace

    void chunk::map_y_to_subchunk(int y, int& index, int& offset) {
        index = y < 0 ? (y - 15) / 16 : y / 16;
        offset = y % 16;
        if (offset < 0) offset += 16;
    }

    block_appearance chunk::get_block_with_color(int cx, int y, int cz, int layer) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return {};
        }
        return it->second->get_block_with_color(cx, offset, cz, layer);
    }

    const std::string& chunk::get_block_name(int cx, int y, int cz, int layer) {
        static const std::string unknown = "minecraft:unknown";
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return unknown;
        }
        return it->second->get_block_name(cx, offset, cz, layer);
    }

    nbt::compound_tag* chunk::get_block_raw(int cx, int y, int cz, int layer) {
        int index;
        int offset;
        map_y_to_subchunk(y, index, offset);
        auto it = this->sub_chunks_.find(index);
        if (it == this->sub_chunks_.end()) {
            return nullptr;
        }
        return it->second->get_block_raw(cx, offset, cz, layer);
    }

    biome chunk::get_biome(int cx, int y, int cz) { return this->d3d_.get_biome(cx, y, cz); }

    sub_chunk* chunk::ensure_sub_chunk(int y) {
        int index = 0;
        int offset = 0;
        map_y_to_subchunk(y, index, offset);
        if (auto it = this->sub_chunks_.find(index); it != this->sub_chunks_.end()) return it->second;

        auto* created = new bl::sub_chunk();
        created->set_version(is_new_chunk_format(this->chunk_format_) ? SubChunkVersion::V9 : SubChunkVersion::V8);
        created->set_y_index(static_cast<int8_t>(index));
        this->sub_chunks_[index] = created;
        return created;
    }

    void chunk::set_block(int cx, int y, int cz, const nbt::compound_tag* tag, int layer) {
        auto* target = this->ensure_sub_chunk(y);
        if (!target) return;
        int index = 0;
        int offset = 0;
        map_y_to_subchunk(y, index, offset);
        target->set_block(cx, offset, cz, tag, layer);
    }

    void chunk::set_block_entity(int cx, int y, int cz, const nbt::compound_tag* tag) {
        if (!tag) return;
        auto copy = std::unique_ptr<nbt::compound_tag>(static_cast<nbt::compound_tag*>(tag->copy()));
        const int baseX = this->pos_.x * 16;
        const int baseZ = this->pos_.z * 16;
        set_block_entity_pos(copy.get(), {baseX + cx, y, baseZ + cz});
        this->block_entities_.push_back(copy.release());
    }

    bool chunk::add_actor(bedrock_level& level, const nbt::compound_tag* tag, const vec3& world_pos) {
        if (!tag) return false;

        auto* copy = static_cast<nbt::compound_tag*>(tag->copy());
        auto* added = new actor();
        if (!added->load_from_nbt_owned(copy)) {
            delete added;
            delete copy;
            return false;
        }

        added->reassign_uid(static_cast<int64_t>(level.generate_actor_uid()));
        added->set_pos(world_pos.x, world_pos.y, world_pos.z);

        this->entities_.push_back(added);
        return true;
    }

    void chunk::fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer) {
        if (!tag) return;
        const auto area = box.normalized();

        const int x0 = std::max(0, area.min_pos.x);
        const int x1 = std::min(16, area.max_pos.x);
        const int z0 = std::max(0, area.min_pos.z);
        const int z1 = std::min(16, area.max_pos.z);
        if (x0 >= x1 || z0 >= z1 || area.min_pos.y >= area.max_pos.y) return;

        for (int y = area.min_pos.y; y < area.max_pos.y;) {
            int index = 0;
            int offset = 0;
            map_y_to_subchunk(y, index, offset);
            const int count = std::min(16 - offset, area.max_pos.y - y);
            const bl::block_box local{{x0, offset, z0}, {x1, offset + count, z1}};

            const auto it = this->sub_chunks_.find(index);
            sub_chunk* target = nullptr;
            if (it != this->sub_chunks_.end()) {
                target = it->second;
            } else if (layer >= 0) {
                target = this->ensure_sub_chunk(y);
            }
            if (target) target->fill_blocks(local, tag, layer);
            y += count;
        }
    }

    void chunk::compact() {
        for (auto& [index, sub] : this->sub_chunks_) {
            if (sub) sub->compact();
        }

        std::set<std::tuple<int, int, int>> occupied;
        std::vector<nbt::compound_tag*> unique;
        unique.reserve(this->block_entities_.size());
        for (auto it = this->block_entities_.rbegin(); it != this->block_entities_.rend(); ++it) {
            auto* entity = *it;
            int x = 0;
            int y = 0;
            int z = 0;
            const bool hasPosition = entity && read_int_tag(entity, "x", x) && read_int_tag(entity, "y", y) && read_int_tag(entity, "z", z);
            if (hasPosition && !occupied.emplace(x, y, z).second) {
                delete entity;
                continue;
            }
            unique.push_back(entity);
        }
        std::reverse(unique.begin(), unique.end());
        this->block_entities_ = std::move(unique);

        this->refresh_height_map();
    }

    void chunk::refresh_height_map() {
        // Do not replace an unloaded height payload with an all-void map.
        if (this->sub_chunks_.empty()) return;

        const auto [min_y, max_y] = this->get_y_range();
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                const int top_y = this->get_top_y(x, z, max_y).first;
                if (top_y < min_y) {
                    this->d3d_.set_void_height(x, z);
                } else {
                    this->d3d_.set_height(x, z, top_y);
                }
            }
        }
    }

    raw_chunk chunk::to_raw_chunk() {
        this->compact();
        raw_chunk out(this->pos_);
        out.set_chunk_format(this->chunk_format_);
        // The version marker is required and stores a single-byte format value.
        out.set_normal(is_new_chunk_format(this->chunk_format_) ? chunk_key::VersionNew : chunk_key::VersionOld,
                       std::string(1, static_cast<char>(this->chunk_format_)));
        out.set_normal(chunk_key::FinalizedState, FINALIZED_STATE_PAYLOAD);
        for (auto& [index, sub] : this->sub_chunks_) {
            if (!sub) continue;
            out.set_sub_chunk(static_cast<int8_t>(index), sub->to_raw());
        }
        // Keep the biome key consistent with the stored payload layout.
        out.set_normal(this->d3d_.is_3d() ? chunk_key::Data3D : chunk_key::Data2D, this->d3d_.to_raw());
        if (this->block_entities_loaded_) out.set_block_entities(this->block_entities_);
        if (this->entities_loaded_) out.set_entities(this->entities_);
        return out;
    }

    bool chunk::load_subchunks(const bl::raw_chunk& rc) {
        for (auto& [sub_index, raw] : rc.get_sub_chunks()) {
            if (raw.empty()) continue;
            auto* sb = new bl::sub_chunk();
            sb->set_y_index(sub_index);
            if (!sb->load(raw.data(), raw.size())) {
                LOG_F(ERROR, "Can not load sub chunk (pos = %s, idx = %d, data size = %zu)", pos_.to_string().c_str(), sub_index,
                      raw.size());
                delete sb;
                continue;
            }
            this->sub_chunks_[sub_index] = sb;
        }
        return true;
    }

    bool chunk::load_biomes(const bl::raw_chunk& rc) {
        this->d3d_.set_chunk_pos(this->pos_);
        auto raw = rc.get_normal_key(chunk_key::Data3D);
        if (!raw.empty()) return this->d3d_.load_from_d3d(raw.data(), raw.size());
        raw = rc.get_normal_key(chunk_key::Data2D);
        return !raw.empty() && this->d3d_.load_from_d2d(raw.data(), raw.size());
    }

    bool chunk::load_pending_ticks(const bl::raw_chunk& rc) {
        auto raw = rc.get_normal_key(chunk_key::PendingTicks);
        if (!raw.empty()) {
            this->pending_ticks_ = nbt::read_palette_to_end(raw.data(), raw.size());
        }
        return true;
    }

    void chunk::load_entities(const bl::raw_chunk& rc) {
        auto raw = rc.get_normal_key(chunk_key::Entity);
        if (!raw.empty()) {
            auto actors = nbt::read_palette_to_end(raw.data(), raw.size());
            for (auto& a : actors) {
                auto* ac = new actor;
                if (ac->load_from_nbt_owned(a)) {
                    this->entities_.push_back(ac);
                } else {
                    delete ac;
                    delete a;
                }
            }
        }
        const auto& digest_raw = rc.get_actor_digest();
        if (!digest_raw.empty()) {
            bl::actor_digest_list list;
            list.load(digest_raw);
            const auto& rc_entities = rc.get_entities();
            for (auto& uid : list.actor_digests_) {
                auto it = rc_entities.find(uid);
                if (it != rc_entities.end() && !it->second.empty()) {
                    auto ac = new actor;
                    if (!ac->load(it->second.data(), it->second.size())) {
                        delete ac;
                    } else {
                        this->entities_.push_back(ac);
                    }
                } else {
                    if (bl::config::log_mismatched_actor()) {
                        LOG_F(ERROR, "[%s] mismatch found between actor digest and chunk data", pos_.to_string().c_str());
                    }
                }
            }
        }
    }

    void chunk::load_hsa(const bl::raw_chunk& rc) {
        auto raw = rc.get_normal_key(chunk_key::HardCodedSpawnAreas);
        if (raw.empty()) return;
        this->HSAs_.from_raw(raw);
    }

    bool chunk::load_block_entities(const bl::raw_chunk& rc) {
        auto raw = rc.get_normal_key(chunk_key::BlockEntity);
        if (!raw.empty()) {
            this->block_entities_ = nbt::read_palette_to_end(raw.data(), raw.size());
        }
        return true;
    }

    bool chunk::load_data(bedrock_level& level, chunk_load_policy policy) {
        if (this->loaded()) return true;
        bl::raw_chunk rc(this->pos_);
        if (!rc.read(level, policy)) return false;
        return this->load_from_raw_chunk(rc, policy);
    }

    bool chunk::load_from_raw_chunk(const bl::raw_chunk& rc, chunk_load_policy policy) {
        this->chunk_format_ = rc.chunk_format();
        if (has_flag(policy, chunk_load_policy::Terrain)) {
            this->load_subchunks(rc);
            this->load_biomes(rc);
        }
        if (has_flag(policy, chunk_load_policy::Actor)) {
            this->load_entities(rc);
            this->entities_loaded_ = true;
        }
        if (has_flag(policy, chunk_load_policy::BlockActor)) {
            this->load_block_entities(rc);
            this->block_entities_loaded_ = true;
        }
        if (has_flag(policy, chunk_load_policy::PendingTick)) {
            this->load_pending_ticks(rc);
        }
        if (has_flag(policy, chunk_load_policy::Others)) {
            this->load_hsa(rc);
        }
        this->loaded_ = true;
        return this->loaded_;
    }

    std::pair<int, int> chunk::get_y_range() const {
        if (this->sub_chunks_.empty()) return {0, -1};
        const int first = this->sub_chunks_.begin()->first;
        const int last = this->sub_chunks_.rbegin()->first;
        return {first * 16, last * 16 + 15};
    }

    int chunk::get_height(int cx, int cz) { return this->d3d_.height(cx, cz); }

    std::pair<int, int> chunk::get_top_y(int cx, int cz, int max_y) {
        const auto [min_y, _max] = get_y_range();
        int top_y = min_y - 1;
        int solid_y = min_y - 1;

        for (int y = max_y; y >= min_y; y--) {
            const auto& name = get_block_name(cx, y, cz);  // no per-block string copy
            if (name == "minecraft:unknown") continue;
            if (top_y < min_y && name != "minecraft:air") {
                top_y = y;
            }
            if (name != "minecraft:air" && name != "minecraft:water" && solid_y < min_y) {
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
        for (auto& sub : this->sub_chunks_) {
            delete sub.second;
        }
        for (auto& p : this->pending_ticks_) delete p;
        for (auto& p : this->block_entities_) delete p;
        for (auto& e : this->entities_) delete e;
    }
}  // namespace bl
