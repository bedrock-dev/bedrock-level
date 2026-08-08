//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_CHUNK_H
#define BEDROCK_LEVEL_CHUNK_H

// cached chunks

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "actor.h"
#include "bedrock_key.h"
#include "data_3d.h"
#include "leveldb/write_batch.h"
#include "sub_chunk.h"
#include "utils.h"

namespace bl {

    class bedrock_level;

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
        explicit raw_chunk(const chunk_pos &pos) : pos_(pos) {}

        raw_chunk() = default;
        raw_chunk(const raw_chunk &other) = default;

        [[nodiscard]] bool loaded() const {
            return std::any_of(data_.begin(), data_.end(), [](const auto &p) { return p.second.size() > 0; });
        }

        [[nodiscard]] ChunkVersion version() const {
            return get_normal_key(chunk_key::VersionNew).empty() ? ChunkVersion::Old : ChunkVersion::New;
        }

        void clear_terrain();

        void clear_entities();

        // read raw chunk from leveldb
        bool read(bedrock_level &level, chunk_load_policy policy = chunk_load_policy::All);

        // write raw chunk to leveldb
        bool write(leveldb::WriteBatch &batch, bool clear);

        // seri and deseri (custom format)
        std::vector<byte_t> to_raw();
        bool from_raw(const std::vector<byte_t> &data);

        // getter
        std::string get_normal_key(chunk_key::key_type key) const;
        std::string get_sub_chunk(int8_t yindex) const;
        const std::map<chunk_key::key_type, std::string> &get_normal_data() const { return data_; }
        const std::map<int8_t, std::string> &get_sub_chunks() const { return sub_chunk_data_; }
        const std::string &get_actor_digest() const { return actor_digest_; }
        const std::map<std::string, std::string> &get_entities() const { return entities_; }
        const chunk_pos &pos() const { return pos_; }

        // setter
        void set_pos(const bl::chunk_pos &pos, bedrock_level *level);
        void set_normal(chunk_key::key_type key, const std::string &data) { data_[key] = data; }
        void set_entities(const std::vector<bl::actor *> actors);

        void set_biome(biome biome);

       private:
        chunk_pos pos_;
        std::map<chunk_key::key_type, std::string> data_;
        std::map<int8_t, std::string> sub_chunk_data_;
        std::string actor_digest_;
        std::map<std::string, std::string> entities_;
    };

    class chunk {
       public:
        friend class bedrock_level;
        static bool valid_in_chunk_pos(int cx, int y, int cz, int dim);
        static void map_y_to_subchunk(int y, int &index, int &offset);
        static std::tuple<int, int> subchunk_index_to_y_range(int y);

       public:
        [[nodiscard]] inline bool fast_load() const { return this->fast_load_mode_; }

        block_info get_block(int cx, int y, int cz);

        block_info get_block_fast(int cx, int y, int cz);

        /// Block name without copying (lives as long as the chunk); "minecraft:unknown" on miss
        [[nodiscard]] const std::string &get_block_name(int cx, int y, int cz);

        palette::compound_tag *get_block_raw(int cx, int y, int cz);

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        [[nodiscard]] bl::chunk_pos get_pos() const;

        int get_height(int cx, int cz);

        std::pair<int, int> get_top_y(int cx, int cz, int max_y);

        explicit chunk(const chunk_pos &pos) : loaded_(false), pos_(pos) {};

        chunk() = delete;

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }
        std::vector<bl::palette::compound_tag *> &block_entities() { return this->block_entities_; }
        std::vector<bl::palette::compound_tag *> &pending_ticks() { return this->pending_ticks_; }

        std::vector<bl::actor *> entities() & { return this->entities_; }

        hardcoded_spawn_area_list &HSAs() { return this->HSAs_; }

        [[nodiscard]] ChunkVersion get_version() const { return this->version; }

       public:
        bool load_from_raw_chunk(const bl::raw_chunk &rc, chunk_load_policy policy = chunk_load_policy::All);

        ~chunk();

       private:
        bool load_data(bedrock_level &level, chunk_load_policy policy);

       private:
        bool load_subchunks(const bl::raw_chunk &rc);

        bool load_biomes(const bl::raw_chunk &rc);

        void load_entities(const bl::raw_chunk &rc);

        bool load_pending_ticks(const bl::raw_chunk &rc);

        bool load_block_entities(const bl::raw_chunk &rc);

        void load_hsa(const bl::raw_chunk &rc);

        bool loaded_{false};
        const chunk_pos pos_;
        // sub_chunks
        std::map<int, sub_chunk *> sub_chunks_;
        // biome and height map
        biome3d d3d_{};
        // actor digest
        //        bl::actor_digest_list actor_digest_list_;
        // block entities
        std::vector<bl::actor *> entities_;
        std::vector<bl::palette::compound_tag *> block_entities_;
        std::vector<bl::palette::compound_tag *> pending_ticks_;

        bl::hardcoded_spawn_area_list HSAs_;
        ChunkVersion version{New};
        bool fast_load_mode_{false};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_H
