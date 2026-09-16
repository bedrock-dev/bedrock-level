//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_RAW_CHUNK_H
#define BEDROCK_LEVEL_RAW_CHUNK_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bedrock_key.h"
#include "leveldb/write_batch.h"
#include "utils.h"

namespace bl {

    class bedrock_level;

    namespace nbt {
        class compound_tag;
    }  // namespace nbt

    // Matches the declaration in data_3d.h; kept as a forward declaration so this header
    // does not pull in the biome table.
    enum biome : uint8_t;

    // Only ever held by pointer here, so the full definition is not needed.
    class actor;

    // bitmask of chunk data to read/parse; combine with | (default All)
    enum chunk_load_policy : uint8_t {
        Terrain = 1 << 0,      // subchunks + biome/height map
        PendingTick = 1 << 1,  // pending ticks key
        Actor = 1 << 2,        // entities (actors) + digest
        BlockActor = 1 << 3,   // block entities key
        Others = 1 << 4,       // version keys, HSA and remaining normal keys
        All = Terrain | PendingTick | Actor | BlockActor | Others
    };

    constexpr chunk_load_policy operator|(chunk_load_policy a, chunk_load_policy b) {
        return static_cast<chunk_load_policy>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr bool has_flag(chunk_load_policy value, chunk_load_policy flag) {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    // all keys-values from level, without parse
    class raw_chunk {
       public:
        // version/terrain marker keys that always gate chunk validity
        inline static const chunk_key::key_type MARKER_KEYS[] = {chunk_key::VersionOld, chunk_key::VersionNew, chunk_key::LegacyTerrain};

        explicit raw_chunk(const chunk_pos& pos) : pos_(pos) {}

        raw_chunk() = default;
        raw_chunk(const raw_chunk& other) = default;

        [[nodiscard]] ChunkVersion version() const { return this->version_; }

        void clear_terrain();
        void clear_entities();

        // read raw chunk from leveldb
        bool read(bedrock_level& level, chunk_load_policy policy = chunk_load_policy::All);

        // write raw chunk to leveldb
        bool write(leveldb::WriteBatch& batch, bool clear);

        // seri and deseri (custom format)
        std::vector<byte_t> to_raw();
        bool from_raw(const std::vector<byte_t>& data);

        // getter
        std::string get_normal_key(chunk_key::key_type key) const;
        std::string get_sub_chunk(int8_t yindex) const;
        const std::map<chunk_key::key_type, std::string>& get_normal_data() const { return data_; }
        const std::map<int8_t, std::string>& get_sub_chunks() const { return sub_chunk_data_; }
        const std::string& get_actor_digest() const { return actor_digest_; }
        const std::map<std::string, std::string>& get_entities() const { return entities_; }
        const chunk_pos& pos() const { return pos_; }

        /// World Y range covered by the SubChunkTerrain payloads this chunk holds. Derived from
        /// the stored keys, so it reflects the real data instead of a version convention.
        /// Returns {0, -1} (empty/inverted) when no terrain has been read.
        [[nodiscard]] std::pair<int, int> get_y_range() const;

        // setter
        void set_pos(const bl::chunk_pos& pos, bedrock_level* level);
        void set_normal(chunk_key::key_type key, const std::string& data) { data_[key] = data; }
        /// Replaces the SubChunkTerrain payload at yindex (empty data deletes the key on write).
        void set_sub_chunk(int8_t yindex, std::string data) { sub_chunk_data_[yindex] = std::move(data); }
        /// Replaces the entity payload. version picks the storage layout -- Old concatenates the
        /// tags into the Entity key, New writes one "actorprefix<key>" entry per actor plus the
        /// digest -- and is the caller's chunk format: a raw_chunk that was never read from the
        /// level does not know its own, so its version_ default must not decide this.
        void set_entities(const std::vector<bl::actor*> actors, ChunkVersion version);

        /// Replaces the BlockEntity payload with the raw NBT of each tag, concatenated the way
        /// the chunk stores them. An empty list clears the payload, which removes the key.
        void set_block_entities(const std::vector<nbt::compound_tag*>& entities);

        void set_biome(biome biome);

        /// Replaces the biome/height payload of out's existing biome key: Data3D for the 3D
        /// layout, Data2D for the legacy one. The payload has to match the key, and a chunk
        /// that has neither key has nowhere to put it, so it is left alone.
        void set_biome_data(const std::string& payload, bool use_3d);

       private:
        chunk_pos pos_;
        std::map<chunk_key::key_type, std::string> data_;
        std::map<int8_t, std::string> sub_chunk_data_;
        std::string actor_digest_;
        std::map<std::string, std::string> entities_;
        ChunkVersion version_{Old};
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_RAW_CHUNK_H
