//
// Created by xhy on 2023/3/30.
//

#include "bedrock_key.h"
#include "utils.h"

namespace bl {


    const bedrock_key  bedrock_key::INVALID_KEY = bedrock_key{bedrock_key::Unknown, 0, 0, 0, 0};


    bedrock_key bedrock_key::parse_key(const std::string &key) {
        if (key.size() < 9) {
            BL_ERROR("Unknown key (%s)", key.c_str());
            return INVALID_KEY;
        }


        auto x = *reinterpret_cast<const int *>(key.data());
        auto z = *reinterpret_cast<const int *>(key.data() + 4);
        auto dim = int{0};
        int8_t y_index = 0;
        key_type type = Unknown;
        if (key.size() == 9 || key.size() == 10) {
            type = static_cast<key_type>(key[8]);
            if (key.size() == 10) {
                y_index = key[9];
            }

        } else if (key.size() == 13 || key.size() == 14) {
            type = static_cast<key_type>(key[12]);
            dim = *reinterpret_cast<const int *>(key.data() + 8);
            if (key.size() == 14) {
                y_index = key[9];
            }
        } else {
            return INVALID_KEY;
        }
        //check invalid

        if (dim != 0 && dim != 1 && dim != 2) {
            return INVALID_KEY;
        }

        if ((key.size() == 10 || key.size() == 14) && type != SubChunkTerrain) {
            return INVALID_KEY;
        }

        return bedrock_key{type, x, z, dim, y_index};
    }


    std::string bedrock_key::bedrock_key_type_to_str(bl::bedrock_key::key_type key) {
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
        }
        return "Unknown";
    }

    std::string bedrock_key::to_string() const {
        auto chunk_pos = std::to_string(x) + ", " + std::to_string(z);
        auto type_info = bedrock_key_type_to_str(type) + "(" + std::to_string(static_cast<int>(type)) + ")";
        auto index_info = std::string();
        if (type == SubChunkTerrain) {
            index_info = std::to_string(y_index);
        }
        return "[" + chunk_pos + "] (" + std::to_string(dimId) + ") " + type_info + " " + index_info;

    }
}

