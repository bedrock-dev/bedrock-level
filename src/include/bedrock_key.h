//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_KEY_H
#define BEDROCK_LEVEL_BEDROCK_KEY_H

#include <vector>
#include <string>

namespace bl {

    struct bedrock_key {

        [[nodiscard]] std::string to_string() const;

        enum key_type {
            Data3D = 43, // 0x2b (+)
            VersionNew = 44, // 0x2c (,)
            Data2D = 45, // 0x2d (-), height map + biomes
            Data2DLegacy = 46, // 0x2e (.)
            SubChunkTerrain = 47, // 0x2f (/)
            LegacyTerrain = 48, // 0x30 (0)
            BlockEntity = 49,
            Entity = 50,
            PendingTicks = 51,
            BlockExtraData = 52,
            BiomeState = 53,
            FinalizedState = 54,
            BorderBlocks = 56, // Education Edition Feature
            HardCodedSpawnAreas = 57,
            Checksums = 59, // 0x3b (;)
            VersionOld = 118, // 0x76 (v)
            Unknown = -1
        };

        static std::string bedrock_key_type_to_str(bedrock_key::key_type key);

        static bedrock_key parse_key(const std::string &key);


        [[maybe_unused]] const static bedrock_key INVALID_KEY;

        key_type type;
        int x;
        int z;
        int dimId;
        int8_t y_index;


    };

}


#endif //BEDROCK_LEVEL_BEDROCK_KEY_H
