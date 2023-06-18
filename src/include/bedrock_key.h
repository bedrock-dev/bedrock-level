//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_KEY_H
#define BEDROCK_LEVEL_BEDROCK_KEY_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bl {

    struct chunk_pos {
        int32_t x{0};
        int32_t z{0};
        int32_t dim{-1};

        [[nodiscard]] bool valid() const { return this->dim >= 0 && this->dim <= 2; }

        [[nodiscard]] std::string to_string() const;

        chunk_pos() = default;

        bool operator==(const chunk_pos &p) const;

        bool operator<(const chunk_pos &rhs) const;
    };

    struct block_pos {
        int x{};
        int y{};
        int z{};

        block_pos(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}

        [[nodiscard]] chunk_pos to_chunk_pos() const;

        [[nodiscard]] chunk_pos in_chunk_offset() const;
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
            BorderBlocks = 56,  // Education Edition Feature
            HardCodedSpawnAreas = 57,
            RandomTicks = 58,
            Checksums = 59,  // 0x3b (;)
            GenerationSeed = 60,
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

        [[nodiscard]] inline bool valid() const { return this->actor_uid != 0xffffffffffffffff; }

        [[nodiscard]] std::string to_string() const;

        static actor_key parse(const std::string &key);
    };

    struct actor_digest_key {
        chunk_pos cp;

        static actor_digest_key parse(const std::string &key);

        [[nodiscard]] inline bool valid() const { return this->cp.valid(); }

        [[nodiscard]] std::string to_string() const;
    };

    struct village_key {
        enum key_type { INFO, DWELLERS, PLAYERS, POI, Unknown };

        static std::string village_key_type_to_str(key_type t);

        [[nodiscard]] bool valid() const {
            return this->uuid.size() == 36 && this->type != Unknown;
        }

        [[nodiscard]] std::string to_string() const;

        static village_key parse(const std::string &key);

        std::string uuid;
        key_type type{Unknown};
    };

}  // namespace bl

namespace std {

    template <>
    struct hash<bl::chunk_pos> {
        std::size_t operator()(const bl::chunk_pos &k) const {
            size_t hash = 3241;
            hash = 3457689L * hash + k.x;
            hash = 8734625L * hash + k.z;
            hash = 2873465L * hash + k.dim;
            return hash;
        }
    };
}  // namespace std

#endif  // BEDROCK_LEVEL_BEDROCK_KEY_H
