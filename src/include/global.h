#ifndef BEDROCK_LEVEL_GLOBAL_H
#define BEDROCK_LEVEL_GLOBAL_H
#include <array>
#include <string_view>

#include "bedrock_key.h"
#include "memory"
#include "nbt.h"

namespace bl {

    namespace global_key {
        using key_list = std::array<std::string_view, 11>;

        /// Known global LevelDB keys that are stored as miscellaneous data.
        [[nodiscard]] const key_list& other_keys() noexcept;
        [[nodiscard]] bool is_other_key(std::string_view key) noexcept;
    }  // namespace global_key

    class village_data {
       public:
        using village_data_type = std::array<bl::nbt::compound_tag*, 4>;
        using village_table_type = std::array<std::unordered_map<std::string, village_data_type>, 4>;

        void reset(const village_table_type& data);
        void append_village(const bl::village_key& key, std::string_view value);

        inline village_table_type& data() { return this->data_; }
        void clear_data();
        ~village_data();

       private:
        village_table_type data_;
    };

    class general_kv_nbts {
       public:
        void reset(const std::unordered_map<std::string, bl::nbt::compound_tag*>& data);

        void append_nbt(std::string_view key, std::string_view value);
        inline std::unordered_map<std::string, bl::nbt::compound_tag*>& data() { return this->data_; };
        inline const std::unordered_map<std::string, bl::nbt::compound_tag*>& data() const { return this->data_; };
        ~general_kv_nbts();

        void clear_data();

       private:
        std::unordered_map<std::string, bl::nbt::compound_tag*> data_;
    };

}  // namespace bl

class global_data {};

#endif  // BEDROCK_LEVEL_GLOBAL_H
