#ifndef BEDROCK_LEVEL_BEDROCK_LEVEL_H
#define BEDROCK_LEVEL_BEDROCK_LEVEL_H
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "bedrock_key.h"
#include "chunk.h"
#include "global.h"
#include "level_dat.h"
#include "leveldb/db.h"
#include "leveldb/options.h"

namespace bl {

    class bedrock_level {
       public:
        /// Select libdeflate instead of zlib for raw-deflate payloads.
        explicit bedrock_level(bool libdeflate = false);
        ~bedrock_level();

        bool open(const std::string& root);
        void close();

        bool is_open() const { return this->is_open_; }
        leveldb::DB*& db() { return this->db_; }
        std::string root_path() const { return this->root_name_; }

        general_kv_nbts& player_data() { return this->player_data_; }
        bl::village_data& village_data() { return this->village_data_; }
        bl::general_kv_nbts& map_item_data() { return this->map_item_data_; }
        bl::general_kv_nbts& other_item_data() { return this->other_data_; }
        level_dat& dat() { return this->dat_; }
        [[nodiscard]] LevelChunkFormat chunk_format() const { return this->chunk_format_; }

        /// Whether this level uses Data3D biome maps.
        [[nodiscard]] bool use_3d_biome_maps() const {
            const std::array<int, 5> v = this->dat_.min_compat_version().version;
            return std::array<int, 3>{v[0], v[1], v[2]} >= std::array<int, 3>{1, 18, 0};
        }

        const std::unordered_map<std::string, int>& custom_dimension_table() const { return custom_dimension_table_; }
        chunk* get_chunk(const chunk_pos& cp, chunk_load_policy policy = chunk_load_policy::All);

        bool load_raw(const std::string& key, std::string& value);
        void load_global_data();

        /// Read options for large database traversals.
        [[nodiscard]] leveldb::ReadOptions bulk_read_options() const;

        void foreach_global_keys(const std::function<void(const std::string&, const std::string&)>& f);
        void foreach_key_with_prefix(const std::string& prefix, const std::function<void(const std::string&, const std::string&)>& f,
                                     std::atomic_bool& stop, int max = -1);

        /// Generate a non-persistent actor ID unique to this process instance.
        uint64_t generate_actor_uid();

        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;
        static const std::string CUSTOM_DIM_KEY_PREFIX;
        static const std::string CUSTOM_DIM_TABLE_KEY;

       private:
        chunk* load_chunk(const bl::chunk_pos& cp, chunk_load_policy policy);
        bool load_db();
        void load_dimension_name_id_table();
        /// Draw the high half of generated actor IDs.
        void roll_actor_uid_tag();

       private:
        leveldb::Options options_{};
        leveldb::ReadOptions read_option_{};

        bool is_open_{false};
        leveldb::DB* db_{nullptr};
        std::string root_name_;
        level_dat dat_;
        LevelChunkFormat chunk_format_{LevelChunkFormat::V9_00};
        bl::village_data village_data_;
        bl::general_kv_nbts player_data_;
        bl::general_kv_nbts map_item_data_;
        bl::general_kv_nbts other_data_;
        std::unordered_map<std::string, int> custom_dimension_table_;

        // High half of generated actor IDs.
        uint32_t actor_uid_tag_{0};
        // Low half; imports can generate IDs on worker threads.
        std::atomic<uint64_t> actor_uid_index{1};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
