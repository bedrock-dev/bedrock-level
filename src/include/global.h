//
// Created by xhy on 2023/3/31.
//

#ifndef BEDROCK_LEVEL_GLOBAL_H
#define BEDROCK_LEVEL_GLOBAL_H
#include "bedrock_key.h"
#include "memory"
#include "palette.h"
namespace bl {
    class autonomous_entities {};

    class biome_data {};

    class level_chunk_metadata_dictionary {};

    class village_data {
       public:
        void reset(
            const std::unordered_map<std::string, std::array<bl::palette::compound_tag*, 4>>& data);
        void append_village(const bl::village_key& key, const std::string& value);

        inline std::unordered_map<std::string, std::array<bl::palette::compound_tag*, 4>>& data() {
            return this->data_;
        }

        ~village_data();

       private:
        std::unordered_map<std::string, std::array<bl::palette::compound_tag*, 4>> data_;
    };

    class player_data {
       public:
        void reset(const std::unordered_map<std::string, bl::palette::compound_tag*>& data);

        void append_player(const std::string& key, const std::string& value);
        inline std::unordered_map<std::string, bl::palette::compound_tag*>& data() {
            return this->data_;
        };
        ~player_data();

       private:
        std::unordered_map<std::string, bl::palette::compound_tag*> data_;
    };

}  // namespace bl

class global_data {};

#endif  // BEDROCK_LEVEL_GLOBAL_H
