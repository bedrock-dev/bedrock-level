//
// Created by xhy on 2023/3/31.
//

#ifndef BEDROCK_LEVEL_GLOBAL_H
#define BEDROCK_LEVEL_GLOBAL_H
#include <array>

#include "bedrock_key.h"
#include "memory"
#include "palette.h"

namespace bl {

    class village_data {
        using village_data_type = std::array<bl::palette::compound_tag*, 4>;
        using village_table_type =
            std::array<std::unordered_map<std::string, village_data_type>, 4>;

       public:
        void reset(const village_table_type& data);
        void append_village(const bl::village_key& key, const std::string& value);

        inline village_table_type& data() { return this->data_; }
        void clear_data();
        ~village_data();

       private:
        village_table_type data_;
    };

    class general_kv_nbts {
       public:
        void reset(const std::unordered_map<std::string, bl::palette::compound_tag*>& data);

        void append_nbt(const std::string& key, const std::string& value);
        inline std::unordered_map<std::string, bl::palette::compound_tag*>& data() {
            return this->data_;
        };
        inline const std::unordered_map<std::string, bl::palette::compound_tag*>& data() const {
            return this->data_;
        };
        ~general_kv_nbts();

        void clear_data();

       private:
        std::unordered_map<std::string, bl::palette::compound_tag*> data_;
    };

}  // namespace bl

class global_data {};

#endif  // BEDROCK_LEVEL_GLOBAL_H
