//
// Created by xhy on 2023/6/21.
//

#ifndef BEDROCK_LEVEL_LEVEL_DAT_H
#define BEDROCK_LEVEL_LEVEL_DAT_H
#include "bedrock_key.h"
#include "nbt-cpp/nbt.hpp"
namespace bl {
    class level_dat {
       public:
        bool load(const std::string& path);

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }
        [[nodiscard]] inline block_pos spawn_position() const { return this->spawn_position_; }
        [[nodiscard]] inline uint64_t storage_version() const { return this->storage_version_; }
        [[nodiscard]] inline std::string level_name() const { return this->level_name_; }

        [[nodiscard]] const nbt::tags::compound_tag* root() const { return this->root_; }

       private:
        bool preload_data();
        bool loaded_{false};
        block_pos spawn_position_{0, 0, 0};
        //        uint64_t seed_{0};
        std::string level_name_;
        int storage_version_{10};
        nbt::tags::compound_tag* root_{nullptr};
        //  Add others if needed
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_LEVEL_DAT_H
