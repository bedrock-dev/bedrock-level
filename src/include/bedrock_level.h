//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_LEVEL_H
#define BEDROCK_LEVEL_BEDROCK_LEVEL_H

#include <string>

#include "leveldb/db.h"
#include "nbt.hpp"

namespace bl {

    class bedrock_level {
       public:
        [[nodiscard]] bool is_open() const { return this->is_open_; }

        bool open(const std::string &root);

        // for dev
        void parse_keys();

        leveldb::DB *&db() { return this->db_; }

        inline void close() { delete this->db_; }

        ~bedrock_level();

       private:
        bool is_open_{false};
        leveldb::DB *db_{nullptr};
        std::string root_name_;
        nbt::tags::compound_tag level_dat_;

       private:
        bool read_level_dat();

        bool read_db();

        static const std::string LEVEL_DATA;
        static const std::string LEVEL_DB;
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_BEDROCK_LEVEL_H
