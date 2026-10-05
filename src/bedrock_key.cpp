#include "bedrock_key.h"

#include <iomanip>
#include <sstream>
#include <type_traits>

#include "binary_io.h"
#include "magic-enum/magic_enum.hpp"
#include "utils.h"

namespace bl {
    const chunk_key chunk_key::INVALID_CHUNK_KEY = chunk_key{chunk_key::Unknown, bl::chunk_pos(), 0, 0};

    namespace {

        [[nodiscard]] inline bool is_known_chunk_key_type(unsigned char value) noexcept {
            return (value >= static_cast<unsigned char>(chunk_key::Data3D) &&
                    value <= static_cast<unsigned char>(chunk_key::ActorDigestVersion)) ||
                   value == static_cast<unsigned char>(chunk_key::VersionOld) ||
                   value == static_cast<unsigned char>(chunk_key::AabbVolumes) ||
                   value == static_cast<unsigned char>(chunk_key::JigsawStructureBlueprint);
        }

        template <typename T>
        [[nodiscard]] T read_key_value(const char* data) noexcept {
            static_assert(std::is_integral_v<T>, "Chunk key values must be integral");
            if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
                return static_cast<T>(binary::read_u32_le(data));
            } else {
                static_assert(sizeof(T) == sizeof(std::uint64_t), "Unsupported chunk key value size");
                return static_cast<T>(binary::read_u64_le(data));
            }
        }

        [[nodiscard]] inline chunk_key parse_jigsaw_key(std::string_view key) noexcept {
            if (key.size() != 21) return chunk_key::INVALID_CHUNK_KEY;

            const auto type_value = static_cast<unsigned char>(key[12]);
            if (type_value != static_cast<unsigned char>(chunk_key::JigsawStructureBlueprint)) {
                return chunk_key::INVALID_CHUNK_KEY;
            }

            const chunk_pos position{read_key_value<std::int32_t>(key.data()), read_key_value<std::int32_t>(key.data() + 4),
                                     read_key_value<std::int32_t>(key.data() + 8)};
            const auto identifier_hash = read_key_value<std::uint64_t>(key.data() + 13);
            return chunk_key{chunk_key::JigsawStructureBlueprint, position, 0, identifier_hash};
        }

        [[nodiscard]] inline chunk_key parse_standard_chunk_key(std::string_view key) noexcept {
            const bool has_dimension = key.size() == 13 || key.size() == 14;
            const bool has_y_index = key.size() == 10 || key.size() == 14;
            if (!has_dimension && key.size() != 9 && key.size() != 10) return chunk_key::INVALID_CHUNK_KEY;

            const auto type_offset = has_dimension ? 12u : 8u;
            const auto type_value = static_cast<unsigned char>(key[type_offset]);
            if (!is_known_chunk_key_type(type_value) || type_value == static_cast<unsigned char>(chunk_key::JigsawStructureBlueprint)) {
                return chunk_key::INVALID_CHUNK_KEY;
            }

            const auto type = static_cast<chunk_key::key_type>(type_value);
            if (has_y_index && type != chunk_key::SubChunkTerrain) return chunk_key::INVALID_CHUNK_KEY;

            const chunk_pos position{read_key_value<std::int32_t>(key.data()), read_key_value<std::int32_t>(key.data() + 4),
                                     has_dimension ? read_key_value<std::int32_t>(key.data() + 8) : 0};
            const auto y_index = has_y_index ? static_cast<std::int8_t>(key.back()) : static_cast<std::int8_t>(0);
            return chunk_key{type, position, y_index, 0};
        }

    }  // namespace

    chunk_key chunk_key::parse(std::string_view key) {
        if (key.size() == 21) return parse_jigsaw_key(key);
        return parse_standard_chunk_key(key);
    }

    actor_key actor_key::parse(std::string_view key) {
        actor_key res;
        if (key.size() != storage_key::actor.size() + sizeof(res.actor_uid) || key.rfind(storage_key::actor, 0) != 0) return res;
        res.actor_uid = binary::read_u64_le(key.data() + storage_key::actor.size());
        return res;
    }

    actor_digest_key actor_digest_key::parse(std::string_view key) {
        actor_digest_key res{};
        if (key.size() != storage_key::actor_digest.size() + 8 && key.size() != storage_key::actor_digest.size() + 12) return res;
        if (key.rfind(storage_key::actor_digest, 0) != 0) return res;
        res.cp.x = binary::read_i32_le(key.data() + storage_key::actor_digest.size());
        res.cp.z = binary::read_i32_le(key.data() + storage_key::actor_digest.size() + sizeof(res.cp.x));
        res.cp.dim = 0;
        if (key.size() == storage_key::actor_digest.size() + 12) {
            res.cp.dim = binary::read_i32_le(key.data() + storage_key::actor_digest.size() + 2 * sizeof(res.cp.x));
        }
        return res;
    }

    std::string actor_digest_key::to_string() const { return this->cp.to_string(); }
    std::string actor_digest_key::to_raw() const {
        if (!this->cp.valid()) return "";
        size_t sz = 8;
        if (cp.dim != 0) sz = 12;
        std::string res{storage_key::actor_digest};
        std::string r;
        r.reserve(sz);
        binary::append_i32_le(r, cp.x);
        binary::append_i32_le(r, cp.z);
        if (this->cp.dim != 0) {
            binary::append_i32_le(r, cp.dim);
        }
        return res + r;
    }
    village_key village_key::parse(std::string_view key) {
        village_key res;

        constexpr std::string_view prefix = storage_key::village;
        if (!key.starts_with(prefix)) return res;
        const auto rest = key.substr(prefix.size());
        const auto first_separator = rest.find('_');
        if (first_separator == std::string_view::npos) return res;
        const auto second_separator = rest.find('_', first_separator + 1);
        if (second_separator != std::string_view::npos && rest.find('_', second_separator + 1) != std::string_view::npos) return res;

        std::string_view uuid;
        std::string_view type_str;
        if (second_separator == std::string_view::npos) {
            uuid = rest.substr(0, first_separator);
            type_str = rest.substr(first_separator + 1);
        } else {
            uuid = rest.substr(first_separator + 1, second_separator - first_separator - 1);
            type_str = rest.substr(second_separator + 1);
            const auto dim_str = rest.substr(0, first_separator);
            if (dim_str == "Nether") {
                res.dim = 1;
            } else if (dim_str == "TheEnd") {
                res.dim = 2;
            }
        }
        if (uuid.size() != 36 || type_str.empty()) return res;
        res.uuid.assign(uuid);
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
        // Preserve the dimension segment so parse(to_raw(k)) round-trips.
        if (this->dim == 1 || this->dim == 2) {
            return std::string(storage_key::village) + (this->dim == 1 ? "Nether" : "TheEnd") + "_" + this->uuid + "_" +
                   village_key_type_to_str(this->type);
        }
        return std::string(storage_key::village) + this->uuid + "_" + village_key_type_to_str(this->type);
    }
    std::string village_key::village_key_type_to_str(village_key::key_type t) {
        auto name = magic_enum::enum_name(t);
        return name.empty() ? "UNKNOWN" : std::string(name);
    }

    std::string chunk_key::chunk_key_to_str(bl::chunk_key::key_type key) {
        auto name = magic_enum::enum_name(key);
        return name.empty() ? "Unknown" : std::string(name);
    }

    std::string chunk_key::to_string() const {
        auto type_info = chunk_key_to_str(type) + "(" + std::to_string(static_cast<int>(type)) + ")";
        auto index_info = std::string();
        if (type == SubChunkTerrain) {
            index_info = "y = " + std::to_string(y_index);
        } else if (type == JigsawStructureBlueprint) {
            std::ostringstream stream;
            stream << "id hash = 0x" << std::hex << std::setw(16) << std::setfill('0') << identifier_hash;
            index_info = stream.str();
        }

        return "[" + this->cp.to_string() + "] " + type_info + " " + index_info;
    }

    std::string chunk_key::to_raw() const {
        size_t sz = 9;
        if (this->type == SubChunkTerrain) sz += 1;
        if (this->cp.dim != 0 || this->type == JigsawStructureBlueprint) sz += 4;
        if (this->type == JigsawStructureBlueprint) sz += sizeof(identifier_hash);
        std::string r;
        r.reserve(sz);
        binary::append_i32_le(r, cp.x);
        binary::append_i32_le(r, cp.z);
        if (this->cp.dim != 0) {
            binary::append_i32_le(r, cp.dim);
            r.push_back(static_cast<char>(this->type));
        } else if (this->type == JigsawStructureBlueprint) {
            binary::append_i32_le(r, cp.dim);
            r.push_back(static_cast<char>(this->type));
        } else {
            r.push_back(static_cast<char>(this->type));
        }

        if (this->type == SubChunkTerrain) {
            r.push_back(static_cast<char>(y_index));
        }
        if (this->type == JigsawStructureBlueprint) {
            binary::append_u64_le(r, identifier_hash);
        }
        return r;
    }

    std::string actor_key::to_string() const { return std::to_string(this->actor_uid); }

    std::string village_key::to_string() const { return this->uuid + "," + village_key_type_to_str(this->type); }

    // Each area uses 25 bytes: six int32 coordinates and one type byte.
    static constexpr size_t HSA_AREA_SIZE = 24 + 1;

    bool hardcoded_spawn_area_list::from_raw(std::string_view raw) {
        this->areas_.clear();
        if (raw.size() < 4) return false;
        const int32_t count = binary::read_i32_le(raw.data());
        if (count < 0 || raw.size() != static_cast<size_t>(count) * HSA_AREA_SIZE + 4) return false;

        const char* d = raw.data() + 4;
        this->areas_.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; i++) {
            hardcoded_spawn_area area;
            const char* p = d + static_cast<size_t>(i) * HSA_AREA_SIZE;
            area.min_pos.x = binary::read_i32_le(p);
            area.min_pos.y = binary::read_i32_le(p + 4);
            area.min_pos.z = binary::read_i32_le(p + 8);
            area.max_pos.x = binary::read_i32_le(p + 12);
            area.max_pos.y = binary::read_i32_le(p + 16);
            area.max_pos.z = binary::read_i32_le(p + 20);
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
        binary::append_i32_le(raw, count);
        for (const auto& area : this->areas_) {
            binary::append_i32_le(raw, area.min_pos.x);
            binary::append_i32_le(raw, area.min_pos.y);
            binary::append_i32_le(raw, area.min_pos.z);
            binary::append_i32_le(raw, area.max_pos.x);
            binary::append_i32_le(raw, area.max_pos.y);
            binary::append_i32_le(raw, area.max_pos.z);
            raw.push_back(static_cast<char>(area.type));
        }
        return raw;
    }
}  // namespace bl
