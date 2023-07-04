//
// Created by xhy on 2023/6/21.
//

#ifndef BEDROCK_LEVEL_LEVEL_DAT_H
#define BEDROCK_LEVEL_LEVEL_DAT_H
#include <memory>

#include "bedrock_key.h"
#include "palette.h"
namespace bl {
    class level_dat {
       public:
        bool load_from_file(const std::string& path);

        bool load_from_raw_data(const std::vector<byte_t>& data);

        void set_nbt(bl::palette::compound_tag* tag);

        [[nodiscard]] inline bool loaded() const { return this->loaded_; }
        [[nodiscard]] inline block_pos spawn_position() const { return this->spawn_position_; }
        [[nodiscard]] inline uint64_t storage_version() const { return this->storage_version_; }
        [[nodiscard]] inline std::string level_name() const { return this->level_name_; }

        [[nodiscard]] bl::palette::compound_tag* root() { return this->root_; }

        [[nodiscard]] std::string to_raw() const;

        ~level_dat();
       private:
        bool preload_data();

       private:
        bool loaded_{false};
        block_pos spawn_position_{0, 0, 0};
        std::string level_name_;
        int storage_version_{10};
        bl::palette::compound_tag* root_{nullptr};
        std::string header_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_LEVEL_DAT_H
