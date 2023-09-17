//
// Created by xhy on 2023/3/30.
//

#include "bedrock_key.h"

#include <cstring>
#include <random>

#include "utils.h"
namespace bl {

    const chunk_key chunk_key::INVALID_CHUNK_KEY =
        chunk_key{chunk_key::Unknown, bl::chunk_pos(), 0};

    chunk_key chunk_key::parse(const std::string &key) {
        auto sz = key.size();
        if (sz == 9 || sz == 10 || sz == 13 || sz == 14) {
            auto x = *reinterpret_cast<const int *>(key.data());
            auto z = *reinterpret_cast<const int *>(key.data() + 4);
            auto dim = 0;
            auto key_type_idx = 8;
            if (sz == 13 || sz == 14) {  // nether or the end
                dim = *reinterpret_cast<const int *>(key.data() + 8);
                key_type_idx = 12;
            }

            if (dim < 0 || dim > 2) {  // invalid dimension key
                return INVALID_CHUNK_KEY;
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
        res.actor_uid = *reinterpret_cast<const int64_t *>(key.data() + 11);
        return res;
    }

    actor_digest_key actor_digest_key::parse(const std::string &key) {
        actor_digest_key res{};
        if (key.size() != 12 && key.size() != 16) return res;
        if (key.rfind("digp", 0) != 0) return res;
        res.cp.x = *reinterpret_cast<const int32_t *>(key.data() + 4);
        res.cp.z = *reinterpret_cast<const int32_t *>(key.data() + 8);
        res.cp.dim = 0;
        if (key.size() == 16) {
            res.cp.dim = *reinterpret_cast<const int32_t *>(key.data() + 12);
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
        if (key.size() < 46) return res;
        if (key.rfind("VILLAGE_", 0) != 0) return res;
        res.uuid = std::string(key.begin() + 8, key.begin() + 44);  // uuid
        std::string type_str = std::string(key.data() + 45);
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
        return res;
    }
    std::string village_key::to_raw() const {
        if (!this->valid()) return {};
        return "VILLAGE_" + this->uuid + "_" + village_key_type_to_str(this->type);
    }
    std::string village_key::village_key_type_to_str(village_key::key_type t) {
        switch (t) {
            case INFO:
                return "INFO";
            case DWELLERS:
                return "DWELLERS";
            case PLAYERS:
                return "PLAYERS";
            case POI:
                return "POI";
            case Unknown:
                return "UNKNOWN";
        }
        return "UNKNOWN";
    }

    std::string chunk_key::chunk_key_to_str(bl::chunk_key::key_type key) {
        switch (key) {
            case Data3D:
                return "Data3D";
            case VersionNew:
                return "VersionNew";
            case Data2D:
                return "Data2D";
            case Data2DLegacy:
                return "Data2DLegacy";
            case SubChunkTerrain:
                return "SubChunkTerrain";
            case LegacyTerrain:
                return "LegacyTerrain";
            case BlockEntity:
                return "BlockEntity";
            case Entity:
                return "Entity";
            case PendingTicks:
                return "PendingTicks";
            case BlockExtraData:
                return "BlockExtraData";
            case BiomeState:
                return "BiomeState";
            case FinalizedState:
                return "FinalizedState";
            case BorderBlocks:
                return "BorderBlocks";
            case HardCodedSpawnAreas:
                return "HardCodedSpawnAreas";
            case Checksums:
                return "Checksums";
            case VersionOld:
                return "VersionOld";
            case Unknown:
                return "Unknown";
            case GenerationSeed:
                return "GenerationSeed";
            case BlendingBiomeHeight:
                return "BlendingBiomeHeight";
            case MetaDataHash:
                return "MetaDataHash";
            case BlendingData:
                return "BlendingData";
            case ActorDigestVersion:
                return "ActorDigestVersion";
            case RandomTicks:
                return "RandomTicks";
                break;
        }
        return "Unknown";
    }

    std::string chunk_pos::to_string() const {
        return std::to_string(this->x) + ", " + std::to_string(this->z) + ", " +
               std::to_string(this->dim);
    }

    bool chunk_pos::operator<(const chunk_pos &rhs) const {
        if (x < rhs.x) return true;
        if (rhs.x < x) return false;
        if (z < rhs.z) return true;
        if (rhs.z < z) return false;
        return dim < rhs.dim;
    }

    bool chunk_pos::operator==(const chunk_pos &p) const {
        return this->x == p.x && this->dim == p.dim && this->z == p.z;
    }

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
        return {0, -1};
    }
    std::tuple<int8_t, int8_t> chunk_pos::get_subchunk_index_range(ChunkVersion v) const {
        if (this->dim == 1) return {0, 7};
        if (this->dim == 2) return {0, 15};
        if (this->dim == 0) {
            if (v == New) {
                return {-4, 19};
            } else {
                return {0, 15};
            }
        }
        return {0, -1};
    }

    bool chunk_pos::is_slime() const {
        auto seed = (x * 0x1f1f1f1fu) ^ (uint32_t)z;
        std::mt19937 mt(seed);
        return mt() % 10 == 0;
    }

    std::string chunk_key::to_string() const {
        auto type_info =
            chunk_key_to_str(type) + "(" + std::to_string(static_cast<int>(type)) + ")";
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

    std::string village_key::to_string() const {
        return this->uuid + "," + village_key_type_to_str(this->type);
    }

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
}  // namespace bl
