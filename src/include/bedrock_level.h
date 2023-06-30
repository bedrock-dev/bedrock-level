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
#include "level_dat.h"
#include "leveldb/db.h"
#include "nbt-cpp/nbt.hpp"

namespace bl {

    class bedrock_level {
       public:
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

        chunk *get_chunk(const chunk_pos &cp);

        //        block_info get_block(const bl::block_pos &pos, int dim);

        //        [[nodiscard]] std::tuple<chunk_pos, chunk_pos> get_range(int dim) const;

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

        ~bedrock_level();

       private:
        /**
         * 从数据库中读取一个区块的所有数据
         * @param cp 区块坐标
         * @return
         */
        chunk *read_chunk_from_db(const bl::chunk_pos &cp);

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

        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
