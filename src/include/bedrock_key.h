//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_KEY_H
#define BEDROCK_LEVEL_BEDROCK_KEY_H

#include <string>
#include <vector>

namespace bl {

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
            Entity = 50,
            PendingTicks = 51,
            BlockExtraData = 52,  //?
            BiomeState = 53,
            FinalizedState = 54,
            BorderBlocks = 56,  // Education Edition Feature
            HardCodedSpawnAreas = 57,
            Checksums = 59,  // 0x3b (;)
            GenerationSeed = 60,
            BlendingBiomeHeight = 62,
            MetaDataHash = 63,
            BlendingData = 64,
            ActorDigestVersion = 65,
            VersionOld = 118,  // 0x76 (v)
            Unknown = -1
        };

        static std::string chunk_key_to_str(chunk_key::key_type key);

        static chunk_key parse_chunk_ley(const std::string &key);

        [[maybe_unused]] const static chunk_key INVALID_CHUNK_KEY;

        key_type type;
        int x;
        int z;
        int dimId;
        int8_t y_index;
    };

    struct global_key {};

}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_KEY_H
