//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_KEY_H
#define BEDROCK_LEVEL_BEDROCK_KEY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bl {

    enum ChunkVersion {
        Old = 0,  // 1.12~1.17
        New = 1   // 1.8+
    };

    struct block_pos;
    struct chunk_pos {
        int32_t x{0};
        int32_t z{0};
        int32_t dim{-1};

        chunk_pos(int32_t xx, int32_t zz, int32_t dimension) : x(xx), z(zz), dim(dimension) {}

        chunk_pos() = default;

        [[nodiscard]] bool valid() const { return this->dim >= 0; }

        [[nodiscard]] std::string to_string() const;

        bool operator==(const chunk_pos &p) const;

        bool operator<(const chunk_pos &rhs) const;

        [[nodiscard]] std::tuple<int32_t, int32_t> get_y_range(ChunkVersion v) const;

        [[nodiscard]] std::tuple<int8_t, int8_t> get_subchunk_index_range(ChunkVersion v) const;
        [[nodiscard]] block_pos get_min_pos(ChunkVersion v) const;
        [[nodiscard]] block_pos get_max_pos(ChunkVersion v) const;

        [[nodiscard]] bool is_slime() const;
    };

    struct block_pos {
        int x{};
        int y{};
        int z{};

        block_pos() = default;
        block_pos(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}

        [[nodiscard]] chunk_pos to_chunk_pos() const;

        [[nodiscard]] chunk_pos in_chunk_offset() const;

        [[nodiscard]] bool operator==(const block_pos &rhs) const noexcept { return x == rhs.x && y == rhs.y && z == rhs.z; }
        [[nodiscard]] bool operator!=(const block_pos &rhs) const noexcept { return !(*this == rhs); }
        [[nodiscard]] block_pos operator+(const block_pos &rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
        [[nodiscard]] block_pos operator-(const block_pos &rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
        block_pos &operator+=(const block_pos &rhs) noexcept {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            return *this;
        }
        block_pos &operator-=(const block_pos &rhs) noexcept {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            return *this;
        }
    };

    // Axis-aligned integer block region. The minimum corner is inclusive and
    // the maximum corner is exclusive: [min_pos, max_pos).
    struct block_box {
        block_pos min_pos{0, 0, 0};
        block_pos max_pos{0, 0, 0};

        block_box() = default;
        block_box(const block_pos &minimum, const block_pos &maximum) : min_pos(minimum), max_pos(maximum) {}

        [[nodiscard]] static block_box from_min_and_size(const block_pos &minimum, int size_x, int size_y, int size_z) noexcept {
            return {{minimum.x, minimum.y, minimum.z}, {minimum.x + size_x, minimum.y + size_y, minimum.z + size_z}};
        }

        [[nodiscard]] bool is_valid() const noexcept { return min_pos.x < max_pos.x && min_pos.y < max_pos.y && min_pos.z < max_pos.z; }

        [[nodiscard]] int size_x() const noexcept { return max_pos.x - min_pos.x; }
        [[nodiscard]] int size_y() const noexcept { return max_pos.y - min_pos.y; }
        [[nodiscard]] int size_z() const noexcept { return max_pos.z - min_pos.z; }

        [[nodiscard]] bool contains(const block_pos &pos) const noexcept {
            return pos.x >= min_pos.x && pos.x < max_pos.x && pos.y >= min_pos.y && pos.y < max_pos.y && pos.z >= min_pos.z &&
                   pos.z < max_pos.z;
        }

        [[nodiscard]] block_box normalized() const noexcept {
            return {{std::min(min_pos.x, max_pos.x), std::min(min_pos.y, max_pos.y), std::min(min_pos.z, max_pos.z)},
                    {std::max(min_pos.x, max_pos.x), std::max(min_pos.y, max_pos.y), std::max(min_pos.z, max_pos.z)}};
        }

        [[nodiscard]] block_box intersected(const block_box &rhs) const noexcept {
            return {{std::max(min_pos.x, rhs.min_pos.x), std::max(min_pos.y, rhs.min_pos.y), std::max(min_pos.z, rhs.min_pos.z)},
                    {std::min(max_pos.x, rhs.max_pos.x), std::min(max_pos.y, rhs.max_pos.y), std::min(max_pos.z, rhs.max_pos.z)}};
        }

        [[nodiscard]] block_box translated(int dx, int dy, int dz) const noexcept { return translated(block_pos{dx, dy, dz}); }

        [[nodiscard]] block_box translated(const block_pos &offset) const noexcept { return {min_pos + offset, max_pos + offset}; }
    };

    struct vec3 {
        float x{};
        float y{};
        float z{};

        vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
    };

    struct chunk_key {
        [[nodiscard]] std::string to_string() const;

        // https://github.com/reedacartwright/rbedrock/blob/6d347a67a258dc910148cbca863f15d77db1721c/R/keys.R#L124
        // https://learn.microsoft.com/en-us/minecraft/creator/documents/actorstorage#non-actor-data-chunk-key-ids
        enum key_type {
            Data3D = 43,           // 0x2b (+)
            VersionNew = 44,       // 0x2c (,)
            Data2D = 45,           // 0x2d (-), height map + biomes
            Data2DLegacy = 46,     // 0x2e (.)
            SubChunkTerrain = 47,  // 0x2f (/)
            LegacyTerrain = 48,    //?
            BlockEntity = 49,
            Entity = 50,  // no longer used
            PendingTicks = 51,
            BlockExtraData = 52,  //?
            BiomeState = 53,
            FinalizedState = 54,
            ConversionData = 55,
            BorderBlocks = 56,  // Education Edition Feature
            HardCodedSpawnAreas = 57,
            RandomTicks = 58,
            Checksums = 59,  // 0x3b (;)
            GenerationSeed = 60,
            GeneratedPreCavesAndCliffsBlending = 61,
            BlendingBiomeHeight = 62,
            MetaDataHash = 63,
            BlendingData = 64,
            ActorDigestVersion = 65,
            VersionOld = 118,  // 0x76 (v)
            Unknown = -1
        };

        [[nodiscard]] bool valid() const { return this->cp.valid() && this->type != Unknown; }

        static std::string chunk_key_to_str(chunk_key::key_type key);

        static chunk_key parse(const std::string &key);

        [[maybe_unused]] const static chunk_key INVALID_CHUNK_KEY;

        [[nodiscard]] std::string to_raw() const;

        key_type type{Unknown};
        chunk_pos cp;
        int8_t y_index{};
    };

    struct actor_key {
        int64_t actor_uid{static_cast<int64_t>(0xffffffffffffffff)};

        [[nodiscard]] inline bool valid() const { return this->actor_uid != static_cast<int64_t>(0xffffffffffffffff); }

        [[nodiscard]] std::string to_string() const;

        static actor_key parse(const std::string &key);
    };

    struct actor_digest_key {
        chunk_pos cp;

        static actor_digest_key parse(const std::string &key);

        [[nodiscard]] inline bool valid() const { return this->cp.valid(); }

        [[nodiscard]] std::string to_string() const;

        [[nodiscard]] std::string to_raw() const;
    };

    struct village_key {
        enum key_type { INFO = 0, DWELLERS = 1, PLAYERS = 2, POI = 3, Unknown };

        static std::string village_key_type_to_str(key_type t);

        [[nodiscard]] bool valid() const { return this->uuid.size() == 36 && this->type != Unknown; }

        [[nodiscard]] std::string to_string() const;

        static village_key parse(const std::string &key);

        [[nodiscard]] std::string to_raw() const;

        std::string uuid;
        int dim{0};
        key_type type{Unknown};
    };

    enum HSAType : int8_t { NetherFortress = 1, SwampHut = 2, OceanMonument = 3, PillagerOutpost = 5, Unknown = 6 };
    struct hardcoded_spawn_area {
        HSAType type{Unknown};
        block_pos min_pos{0, 0, 0};
        block_pos max_pos{0, 0, 0};
    };

    // hardcoded spawn areas of a chunk, with (de)serialization for the HardCodedSpawnAreas key
    class hardcoded_spawn_area_list {
       public:
        using iterator = std::vector<hardcoded_spawn_area>::iterator;
        using const_iterator = std::vector<hardcoded_spawn_area>::const_iterator;

        [[nodiscard]] bool empty() const { return areas_.empty(); }
        [[nodiscard]] size_t size() const { return areas_.size(); }
        [[nodiscard]] iterator begin() { return areas_.begin(); }
        [[nodiscard]] iterator end() { return areas_.end(); }
        [[nodiscard]] const_iterator begin() const { return areas_.begin(); }
        [[nodiscard]] const_iterator end() const { return areas_.end(); }

        std::vector<hardcoded_spawn_area> &areas() { return areas_; }
        const std::vector<hardcoded_spawn_area> &areas() const { return areas_; }

        void clear() { areas_.clear(); }
        void add(const hardcoded_spawn_area &area) { areas_.push_back(area); }
        bool remove(size_t idx) {
            if (idx >= areas_.size()) return false;
            areas_.erase(areas_.begin() + static_cast<std::ptrdiff_t>(idx));
            return true;
        }

        // payload layout: int32 count, then count * (min x/y/z, max x/y/z int32s + 1 type byte)
        bool from_raw(const std::string &raw);
        [[nodiscard]] std::string to_raw() const;

       private:
        std::vector<hardcoded_spawn_area> areas_;
    };
}  // namespace bl

namespace std {

    template <>
    struct hash<bl::chunk_pos> {
        size_t operator()(const bl::chunk_pos &cp) const noexcept {
            size_t h1 = hash<int32_t>{}(cp.x);
            size_t h2 = hash<int32_t>{}(cp.z);
            size_t h3 = hash<int32_t>{}(cp.dim);
            return h1 ^ (h2 << 7) ^ (h3 << 15);
        }
    };
}  // namespace std

#endif  // BEDROCK_LEVEL_BEDROCK_KEY_H
