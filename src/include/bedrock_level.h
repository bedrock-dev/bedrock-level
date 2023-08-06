//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_LEVEL_H
#define BEDROCK_LEVEL_BEDROCK_LEVEL_H
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "bedrock_key.h"
#include "chunk.h"
#include "global.h"
#include "level_dat.h"
#include "leveldb/db.h"

namespace bl {

    class bedrock_level {
       public:
        bedrock_level();

        /*
         * 返回存档是否已经正常打开
         */
        [[nodiscard]] bool is_open() const { return this->is_open_; }

        /**
         * 打开存档
         * @param root 存档根目录，即level.dat所在的目录
         * @return
         */
        bool open(const std::string &root);

        // 返回db的引用
        leveldb::DB *&db() { return this->db_; }

        void close();

        chunk *get_chunk(const chunk_pos &cp, bool fast_load = false);
        general_kv_nbts &player_data() { return this->player_data_; }

        bl::village_data &village_data() { return this->village_data_; }

        bl::general_kv_nbts &map_item_data() { return this->map_item_data_; }

        bl::general_kv_nbts &other_item_data() { return this->other_data_; }

        /**
         * 获取缓存的区块的的数量
         * @return
         */
        [[nodiscard]] inline size_t cached_chunk_size() const {
            return this->chunk_data_cache_.size();
        };

        /**
         * 获取level.dat文件的对象wrapper
         * @return
         */
        level_dat &dat() { return this->dat_; }

        /**
         * 开启时，Level会缓存读取过的区块数据，没有容量限制（后面可以改成cache）
         * 关闭时，会清空内容
         * @param enable
         */
        void set_cache(bool enable);

        actor *load_actor(const std::string &raw_uid);

        ~bedrock_level();

        void foreach_global_keys(
            const std::function<void(const std::string &, const std::string &)> &f);

        void load_global_data();
        // write
        bool remove_chunk(const chunk_pos &cp);

        [[nodiscard]] std::string root_path() const { return this->root_name_; }
        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;

       private:
        /**
         * 从数据库中读取一个区块的所有数据
         * @param cp 区块坐标
         * @return
         */
        chunk *read_chunk_from_db(const bl::chunk_pos &cp, bool fast_load);

        void clear_cache();

       private:
        bool is_open_{false};

        level_dat dat_;

        leveldb::DB *db_{nullptr};

        std::string root_name_;

        std::map<chunk_pos, chunk *> chunk_data_cache_;

       private:
        bool read_db();

        bool enable_cache_{false};
        leveldb::Options options_{};
        // global data
        bl::village_data village_data_;
        bl::general_kv_nbts player_data_;
        bl::general_kv_nbts map_item_data_;
        bl::general_kv_nbts other_data_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
