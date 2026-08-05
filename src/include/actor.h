//
// Created by xhy on 2023/3/31.
//

#ifndef BEDROCK_LEVEL_ACRTOR_H
#define BEDROCK_LEVEL_ACRTOR_H
#include <cstdint>

#include "bedrock_key.h"
#include "palette.h"
#include "utils.h"

namespace bl {
    class bedrock_level;
    class actor {
       public:
        bool load(const byte_t* data, size_t len);

        bool load_from_nbt(bl::palette::compound_tag* nbt);

        /// Take ownership of nbt (no deep copy); on failure the caller keeps ownership
        bool load_from_nbt_owned(bl::palette::compound_tag* nbt);

        /// Offset the entity's Pos x/z by (dx, dz), modify nbt in place
        void offset_pos(float dx, float dz);

        void reassign_uid(int64_t uid);

        [[nodiscard]] inline int64_t uid() const { return this->uid_; }

        [[nodiscard]] int64_t storage_key() const {
            auto u = static_cast<uint64_t>(this->uid_);
            auto wsc = u >> 32;
            auto index = u & 0x00000000ffffffff;
            auto key = static_cast<int64_t>(((0x00000000FFFFFFFFULL - wsc) << 32) | index);
            return key;
        }

        [[nodiscard]] std::string storage_key_raw() const {
            auto key = storage_key();
            std::string result(8, '\0');
            for (int i = 0; i < 8; i++) {
                result[i] = static_cast<char>((key >> (56 - i * 8)) & 0xFF);
            }
            return result;
        }

        void dump();
        [[nodiscard]] vec3 pos() const { return this->pos_; };
        [[nodiscard]] std::string identifier() const { return this->identifier_; };
        [[nodiscard]] bl::palette::compound_tag* root() const { return this->root_; }
        actor() = default;

       private:
        bool preload(bl::palette::compound_tag* root);

        bool loaded_ = false;
        int64_t uid_{-1};
        bl::palette::compound_tag* root_{nullptr};
        std::string identifier_{"minecraft:unknown"};
        vec3 pos_{0, 0, 0};

       public:
        ~actor();
    };

    /* 实体摘要信息
     * key - "dige" + chunk_pos.to_raw()
     * value = key*
     * key = "actorprefix" + uid
     */

    struct actor_digest_list {
        bool load(const std::string& raw) {
            if (raw.size() % 8 != 0) return false;
            const size_t actor_num = raw.size() / 8;
            if (actor_num == 0) return true;
            this->actor_digests_.reserve(actor_num);
            auto it = raw.begin();
            for (size_t i = 0; i < actor_num; i++) {
                this->actor_digests_.emplace_back(it, it + 8);
                it += 8;
            }
            return true;
        }
        std::vector<std::string> actor_digests_;
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_ACRTOR_H
