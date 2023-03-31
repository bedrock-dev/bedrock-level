//
// Created by xhy on 2023/3/30.
//

#include "bedrock_key.h"

#include "utils.h"

namespace bl {

    const chunk_key chunk_key::INVALID_CHUNK_KEY = chunk_key{chunk_key::Unknown, 0, 0, 0, 0};

    namespace {

        //
        //        chunk_key parse_global_key_space(const char *data) {
        //            return chunk_key::INVALID_KEY;
        //        }
        //
        //        chunk_key parse_chunk_key_space(const char *data) {
        //
        //        }

    }

    chunk_key chunk_key::parse_chunk_ley(const std::string &key) {
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
            if (type != Data3D && type != SubChunkTerrain && type != BlockEntity
                // add new item dynamically
            ) {
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

            return chunk_key{type, x, z, dim, y_index};
        } else {
            return INVALID_CHUNK_KEY;
        }
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
        }
        return "Unknown";
    }

    std::string chunk_key::to_string() const {
        auto chunk_pos = std::to_string(x) + ", " + std::to_string(z);
        auto type_info =
            chunk_key_to_str(type) + "(" + std::to_string(static_cast<int>(type)) + ")";
        auto index_info = std::string();
        if (type == SubChunkTerrain) {
            index_info = std::to_string(y_index);
        }
        return "[" + chunk_pos + "] (" + std::to_string(dimId) + ") " + type_info + " " +
               index_info;
    }
}  // namespace bl
