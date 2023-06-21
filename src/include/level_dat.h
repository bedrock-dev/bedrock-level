//
// Created by xhy on 2023/6/21.
//

#ifndef BEDROCK_LEVEL_LEVEL_DAT_H
#define BEDROCK_LEVEL_LEVEL_DAT_H
#include "bedrock_key.h"
namespace bl {
    class level_dat {
        bool load(const std::string& path);

       private:
        bool loaded_{false};
        block_pos spawn_position_{0, 0, 0};
        uint64_t seed_{0};
        std::string level_name_;
        int storage_version_{10};
        //  Add others if needed
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_LEVEL_DAT_H
