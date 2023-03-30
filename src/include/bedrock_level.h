//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_BEDROCK_LEVEL_H
#define BEDROCK_LEVEL_BEDROCK_LEVEL_H

#include "leveldb/db.h"
#include <string>
#include "nbt.hpp"

namespace bl {

    class bedrock_level {


    public:

        [[nodiscard]] bool is_open() const { return this->is_open_; }

        bool open(const std::string &root);

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

}


#endif //BEDROCK_LEVEL_BEDROCK_LEVEL_H
