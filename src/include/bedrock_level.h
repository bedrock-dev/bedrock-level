//
// Created by xhy on 2023/3/30.
//

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
        bedrock_level();
        ~bedrock_level();

        // open && close
        bool open(const std::string &root);
        void close();

        // getter (status and option)
        bool is_open() const { return this->is_open_; }
        leveldb::DB *&db() { return this->db_; }
        std::string root_path() const { return this->root_name_; }

        // getter (data)
        general_kv_nbts &player_data() { return this->player_data_; }
        bl::village_data &village_data() { return this->village_data_; }
        bl::general_kv_nbts &map_item_data() { return this->map_item_data_; }
        bl::general_kv_nbts &other_item_data() { return this->other_data_; }
        level_dat &dat() { return this->dat_; }
        const std::unordered_map<std::string, int> &custom_dimension_table() const { return custom_dimension_table_; }
        chunk *get_chunk(const chunk_pos &cp, chunk_load_policy policy = chunk_load_policy::All);

        // read(load)
        bool load_raw(const std::string &key, std::string &value);
        void load_global_data();

        void foreach_global_keys(const std::function<void(const std::string &, const std::string &)> &f);
        void foreach_key_with_prefix(const std::string &prefix, const std::function<void(const std::string &, const std::string &)> &f,
                                     std::atomic_bool &stop, int max = -1);

        // others
        uint64_t generate_actor_uid();

        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;
        static const std::string CUSTOM_DIM_KEY_PREFIX;
        static const std::string CUSTOM_DIM_TABLE_KEY;

       private:
        // read
        chunk *load_chunk(const bl::chunk_pos &cp, chunk_load_policy policy);
        bool load_db();
        void load_dimension_name_id_table();
        // write

       private:
        // option
        leveldb::Options options_{};
        leveldb::ReadOptions read_option_{};

        // status
        bool is_open_{false};
        leveldb::DB *db_{nullptr};
        std::string root_name_;
        // data
        level_dat dat_;
        bl::village_data village_data_;
        bl::general_kv_nbts player_data_;
        bl::general_kv_nbts map_item_data_;
        bl::general_kv_nbts other_data_;
        std::unordered_map<std::string, int> custom_dimension_table_;

        uint64_t wsc_uid{1};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
