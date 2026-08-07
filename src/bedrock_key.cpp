//
// Created by xhy on 2023/3/30.
//

#include "bedrock_key.h"

#include <cstring>
#include <random>

#include "magic-enum/magic_enum.hpp"
#include "utils.h"

namespace bl {
    const chunk_key chunk_key::INVALID_CHUNK_KEY = chunk_key{chunk_key::Unknown, bl::chunk_pos(), 0};

    chunk_key chunk_key::parse(const std::string &key) {
        auto sz = key.size();
        if (sz == 9 || sz == 10 || sz == 13 || sz == 14) {
            int x, z;
            memcpy(&x, key.data(), 4);
            memcpy(&z, key.data() + 4, 4);
            auto dim = 0;
            auto key_type_idx = 8;
            if (sz == 13 || sz == 14) {  // nether or the end
                memcpy(&dim, key.data() + 8, 4);
                key_type_idx = 12;
            }

            auto type = static_cast<chunk_key::key_type>(key[key_type_idx]);

            if ((type < 43 || type > 65) && type != 118) {
                return INVALID_CHUNK_KEY;
            }

            // sub chunk terrain
            int8_t y_index = 0;
            if (key.size() == 10 || key.size() == 14) {
                if (type != SubChunkTerrain) {
                    return INVALID_CHUNK_KEY;
                }
                y_index = key.back();
            }

            chunk_pos cp{x, z, dim};
            return chunk_key{type, cp, y_index};
        } else {
            return INVALID_CHUNK_KEY;
        }
    }

    actor_key actor_key::parse(const std::string &key) {
        actor_key res;
        if (key.size() != 19 || key.rfind("actorprefix", 0) != 0) return res;
        memcpy(&res.actor_uid, key.data() + 11, 8);
        return res;
    }

    actor_digest_key actor_digest_key::parse(const std::string &key) {
        actor_digest_key res{};
        if (key.size() != 12 && key.size() != 16) return res;
        if (key.rfind("digp", 0) != 0) return res;
        memcpy(&res.cp.x, key.data() + 4, 4);
        memcpy(&res.cp.z, key.data() + 8, 4);
        res.cp.dim = 0;
        if (key.size() == 16) {
            memcpy(&res.cp.dim, key.data() + 12, 4);
        }
        return res;
    }

    std::string actor_digest_key::to_string() const { return this->cp.to_string(); }
    std::string actor_digest_key::to_raw() const {
        if (!this->cp.valid()) return "";
        size_t sz = 8;
        if (cp.dim != 0) sz = 12;
        std::string res = "digp";
        std::string r(sz, '\0');
        memcpy(r.data(), &cp.x, 4);
        memcpy(r.data() + 4, &cp.z, 4);
        if (this->cp.dim != 0) {
            memcpy(r.data() + 8, &cp.dim, 4);
        }
        return res + r;
    }
    village_key village_key::parse(const std::string &key) {
        village_key res;

        auto tks = utils::splitStr(key, '_');
        auto sz = tks.size();
        if (sz != 3 && sz != 4) return res;
        if (tks[0] != "VILLAGE") return res;
        auto uuid = tks[sz - 2];
        if (uuid.size() != 36) return res;
        res.uuid = uuid;
        std::string type_str = tks[sz - 1];
        if (type_str == "DWELLERS") {
            res.type = DWELLERS;
        } else if (type_str == "INFO") {
            res.type = INFO;
        } else if (type_str == "PLAYERS") {
            res.type = PLAYERS;
        } else if (type_str == "POI") {
            res.type = POI;
        } else {
            res.type = Unknown;
        }
        if (sz == 4) {
            auto &dim_str = tks[1];
            if (dim_str == "Overworld") {
                res.dim = 0;
            } else if (dim_str == "Nether") {
                res.dim = 1;
            } else if (dim_str == "TheEnd") {
                res.dim = 2;
            }
        }
        return res;
    }
    std::string village_key::to_raw() const {
        if (!this->valid()) return {};
        // keep the dimension segment so parse(to_raw(k)) == k; 3-segment form is kept for dim 0
        if (this->dim == 1 || this->dim == 2) {
            return "VILLAGE_" + std::string(this->dim == 1 ? "Nether" : "TheEnd") + "_" + this->uuid + "_" +
                   village_key_type_to_str(this->type);
        }
        return "VILLAGE_" + this->uuid + "_" + village_key_type_to_str(this->type);
    }
    std::string village_key::village_key_type_to_str(village_key::key_type t) {
        auto name = magic_enum::enum_name(t);
        return name.empty() ? "UNKNOWN" : std::string(name);
    }

    std::string chunk_key::chunk_key_to_str(bl::chunk_key::key_type key) {
        auto name = magic_enum::enum_name(key);
        return name.empty() ? "Unknown" : std::string(name);
    }

    std::string chunk_pos::to_string() const {
        return std::to_string(this->x) + ", " + std::to_string(this->z) + ", " + std::to_string(this->dim);
    }

    bool chunk_pos::operator<(const chunk_pos &rhs) const {
        if (x < rhs.x) return true;
        if (rhs.x < x) return false;
        if (z < rhs.z) return true;
        if (rhs.z < z) return false;
        return dim < rhs.dim;
    }

    bool chunk_pos::operator==(const chunk_pos &p) const { return this->x == p.x && this->dim == p.dim && this->z == p.z; }

    block_pos chunk_pos::get_min_pos(ChunkVersion v) const {
        auto [y, _] = this->get_y_range(v);
        return {this->x * 16, y, this->z * 16};
    }
    block_pos chunk_pos::get_max_pos(ChunkVersion v) const {
        auto [_, y] = this->get_y_range(v);
        return {this->x * 16 + 15, y, this->z * 16 + 15};
    }

    std::tuple<int32_t, int32_t> chunk_pos::get_y_range(ChunkVersion v) const {
        if (this->dim == 1) return {0, 127};
        if (this->dim == 2) return {0, 255};
        if (this->dim == 0) {
            if (v == New) {
                return {-64, 319};
            } else {
                return {0, 255};
            }
        }
        // custom dimensions may have any height; fall back to the 1.18+ world range
        return {-64, 319};
    }
    std::tuple<int8_t, int8_t> chunk_pos::get_subchunk_index_range(ChunkVersion v) const {
        return {-4, 19};
        // if (this->dim == 1) return {0, 7};
        // if (this->dim == 2) return {0, 15};
        // if (this->dim == 0) {
        //     if (v == New) {
        //         return {-4, 19};
        //     } else {
        //         return {0, 15};
        //     }
        // }
        // return {0, -1};
    }

    bool chunk_pos::is_slime() const {
        auto seed = (x * 0x1f1f1f1fu) ^ (uint32_t)z;
        std::mt19937 mt(seed);
        return mt() % 10 == 0;
    }

    std::string chunk_key::to_string() const {
        auto type_info = chunk_key_to_str(type) + "(" + std::to_string(static_cast<int>(type)) + ")";
        auto index_info = std::string();
        if (type == SubChunkTerrain) {
            index_info = "y = " + std::to_string(y_index);
        }

        return "[" + this->cp.to_string() + "] " + type_info + " " + index_info;
    }

    std::string chunk_key::to_raw() const {
        size_t sz = 9;
        if (this->type == SubChunkTerrain) sz += 1;
        if (this->cp.dim != 0) sz += 4;
        std::string r(sz, '\0');
        memcpy(r.data(), &cp.x, 4);
        memcpy(r.data() + 4, &cp.z, 4);
        if (this->cp.dim != 0) {
            memcpy(r.data() + 8, &cp.dim, 4);
            r[12] = this->type;
        } else {
            r[8] = this->type;
        }

        if (this->type == SubChunkTerrain) {
            r[r.size() - 1] = y_index;
        }
        return r;
    }

    std::string actor_key::to_string() const { return std::to_string(this->actor_uid); }

    std::string village_key::to_string() const { return this->uuid + "," + village_key_type_to_str(this->type); }

    chunk_pos block_pos::to_chunk_pos() const {
        auto cx = x < 0 ? x - 15 : x;
        auto cz = z < 0 ? z - 15 : z;
        return {cx / 16, cz / 16, -1};
    }

    chunk_pos block_pos::in_chunk_offset() const {
        auto ox = x % 16;
        auto oz = z % 16;
        if (ox < 0) ox += 16;
        if (oz < 0) oz += 16;
        return {ox, oz, -1};
    }

    // 25 bytes per area: min(x,y,z) + max(x,y,z) as int32s + 1 type byte
    static constexpr size_t HSA_AREA_SIZE = 24 + 1;

    bool hardcoded_spawn_area_list::from_raw(const std::string &raw) {
        this->areas_.clear();
        if (raw.size() < 4) return false;
        int32_t count = 0;
        memcpy(&count, raw.data(), 4);
        if (raw.size() != static_cast<size_t>(count) * HSA_AREA_SIZE + 4) return false;

        const char *d = raw.data() + 4;
        this->areas_.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; i++) {
            hardcoded_spawn_area area;
            const char *p = d + static_cast<size_t>(i) * HSA_AREA_SIZE;
            memcpy(&area.min_pos.x, p, 4);
            memcpy(&area.min_pos.y, p + 4, 4);
            memcpy(&area.min_pos.z, p + 8, 4);
            memcpy(&area.max_pos.x, p + 12, 4);
            memcpy(&area.max_pos.y, p + 16, 4);
            memcpy(&area.max_pos.z, p + 20, 4);
            auto type = static_cast<int8_t>(p[24]);
            if (type == SwampHut || type == OceanMonument || type == NetherFortress || type == PillagerOutpost) {
                area.type = static_cast<HSAType>(type);
            }
            this->areas_.push_back(area);
        }
        return true;
    }

    std::string hardcoded_spawn_area_list::to_raw() const {
        std::string raw;
        raw.reserve(4 + this->areas_.size() * HSA_AREA_SIZE);
        int32_t count = static_cast<int32_t>(this->areas_.size());
        raw.append(reinterpret_cast<const char *>(&count), 4);
        for (const auto &area : this->areas_) {
            raw.append(reinterpret_cast<const char *>(&area.min_pos.x), 4);
            raw.append(reinterpret_cast<const char *>(&area.min_pos.y), 4);
            raw.append(reinterpret_cast<const char *>(&area.min_pos.z), 4);
            raw.append(reinterpret_cast<const char *>(&area.max_pos.x), 4);
            raw.append(reinterpret_cast<const char *>(&area.max_pos.y), 4);
            raw.append(reinterpret_cast<const char *>(&area.max_pos.z), 4);
            raw.push_back(static_cast<char>(area.type));
        }
        return raw;
    }
}  // namespace bl
