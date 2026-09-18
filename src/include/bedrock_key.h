//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_KEY_H
#define BEDROCK_LEVEL_BEDROCK_KEY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "geometry.h"

namespace bl {

    // From Levilamina
    enum class LevelChunkFormat : signed char {
        V9_00 = 0,
        V9_02 = 1,
        V9_05 = 2,
        V17_0 = 3,
        V18_0 = 4,
        VConsole1ToV18_0 = 5,
        V1_2_0 = 6,
        V1_2_0Bis = 7,
        V1_3_0 = 8,
        V1_8_0 = 9,
        V1_9_0 = 10,
        V1_10_0 = 11,
        V1_11_0 = 12,
        V1_11_1 = 13,
        V1_11_2 = 14,
        V1_12_0 = 15,
        V1_14_0 = 16,
        V1_15_0 = 17,
        V1_16_0 = 18,
        V1_16_0Bis = 19,
        V1_16_100 = 20,
        V1_16_100Bis = 21,
        V1_16_210 = 22,
        V1_16_300CavesCliffsPart1 = 23,
        V1_16_300CavesCliffsInternalV1 = 24,
        V1_16_300CavesCliffsPart2 = 25,
        V1_16_300CavesCliffsInternalV2 = 26,
        V1_16_300CavesCliffsPart3 = 27,
        V1_16_300CavesCliffsInternalV3 = 28,
        V1_16_300CavesCliffsPart4 = 29,
        V1_16_300CavesCliffsInternalV4 = 30,
        V1_16_300CavesCliffsPart5 = 31,
        V1_16_300CavesCliffsInternalV5 = 32,
        V1_18_0 = 33,
        V1_18_0Internal = 34,
        V1_18_1 = 35,
        V1_18_1Internal = 36,
        V1_18_2 = 37,
        V1_18_2Internal = 38,
        V1_18_3 = 39,
        V1_18_3IndividualActorStorage = 40,
        V1_21_4 = 41,
        V1_21_120 = 42,
        Count = 43,
    };

    /// Chunks saved from this format on use the 1.18+ layout (negative-Y sub-chunks, 3D biomes).
    [[nodiscard]] constexpr bool is_new_chunk_format(LevelChunkFormat format) noexcept { return format >= LevelChunkFormat::V1_16_210; }

    /// Chunks saved from this format on store each actor under its own "actorprefix" key instead
    /// of concatenating the tags into the Entity key.
    [[nodiscard]] constexpr bool uses_individual_actor_storage(LevelChunkFormat format) noexcept {
        return format >= LevelChunkFormat::V1_18_3IndividualActorStorage;
    }

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

        static chunk_key parse(const std::string& key);

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

        static actor_key parse(const std::string& key);
    };

    struct actor_digest_key {
        chunk_pos cp;

        static actor_digest_key parse(const std::string& key);

        [[nodiscard]] inline bool valid() const { return this->cp.valid(); }

        [[nodiscard]] std::string to_string() const;

        [[nodiscard]] std::string to_raw() const;
    };

    struct village_key {
        enum key_type { INFO = 0, DWELLERS = 1, PLAYERS = 2, POI = 3, Unknown };

        static std::string village_key_type_to_str(key_type t);

        [[nodiscard]] bool valid() const { return this->uuid.size() == 36 && this->type != Unknown; }

        [[nodiscard]] std::string to_string() const;

        static village_key parse(const std::string& key);

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

        std::vector<hardcoded_spawn_area>& areas() { return areas_; }
        const std::vector<hardcoded_spawn_area>& areas() const { return areas_; }

        void clear() { areas_.clear(); }
        void add(const hardcoded_spawn_area& area) { areas_.push_back(area); }
        bool remove(size_t idx) {
            if (idx >= areas_.size()) return false;
            areas_.erase(areas_.begin() + static_cast<std::ptrdiff_t>(idx));
            return true;
        }

        // payload layout: int32 count, then count * (min x/y/z, max x/y/z int32s + 1 type byte)
        bool from_raw(const std::string& raw);
        [[nodiscard]] std::string to_raw() const;

       private:
        std::vector<hardcoded_spawn_area> areas_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_KEY_H
