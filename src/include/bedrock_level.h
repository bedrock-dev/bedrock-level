//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_LEVEL_H
#define BEDROCK_LEVEL_BEDROCK_LEVEL_H
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "bedrock_key.h"
#include "chunk.h"
#include "global.h"
#include "level_dat.h"
#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/write_batch.h"

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
        size_t cached_chunk_size() const { return chunk_data_cache_.size(); };

        // getter (data)
        general_kv_nbts &player_data() { return this->player_data_; }
        bl::village_data &village_data() { return this->village_data_; }
        bl::general_kv_nbts &map_item_data() { return this->map_item_data_; }
        bl::general_kv_nbts &other_item_data() { return this->other_data_; }
        level_dat &dat() { return this->dat_; }
        chunk *get_chunk(const chunk_pos &cp, bool fast_load = false);

        // setter
        void set_cache(bool enable);

        // read(load)
        bool load_raw(const std::string &key, std::string &value);
        actor *load_actor(const std::string &raw_uid);
        void load_global_data();

        void foreach_global_keys(const std::function<void(const std::string &, const std::string &)> &f);

        // write
        bool remove_chunks(std::set<bl::chunk_pos> &positions);

        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;

       private:
        // cache
        void clear_cache();
        // read
        chunk *load_chunk(const bl::chunk_pos &cp, bool fast_load);
        bool load_db();
        // write
        void remove_chunk_in_batch(const chunk_pos &cp, leveldb::WriteBatch &batch);

       private:
        // option
        bool enable_cache_{false};
        leveldb::Options options_{};
        leveldb::ReadOptions read_option_{};

        // status
        bool is_open_{false};
        leveldb::DB *db_{nullptr};
        std::string root_name_;
        std::map<chunk_pos, chunk *> chunk_data_cache_;
        // data
        level_dat dat_;
        bl::village_data village_data_;
        bl::general_kv_nbts player_data_;
        bl::general_kv_nbts map_item_data_;
        bl::general_kv_nbts other_data_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
