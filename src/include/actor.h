//
// Created by xhy on 2023/3/31.
//

#ifndef BEDROCK_LEVEL_ACRTOR_H
#define BEDROCK_LEVEL_ACRTOR_H
#include "bedrock_key.h"
#include "palette.h"
#include "utils.h"
namespace bl {
    class actor {
       public:
        bool load(const byte_t* data, size_t len);

        bool load_from_nbt(bl::palette::compound_tag* nbt);

        [[nodiscard]] inline int64_t uid() const { return this->uid_; }
        [[nodiscard]] inline std::string uid_raw() const {
            std::string res(8, 0);
            memcpy(res.data(), &this->uid_, 8);
            return res;
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
            for (auto i = 0u; i < actor_num; i++) {
                this->actor_digests_.emplace_back(raw.begin() + i, raw.begin() + i + 8);
            }
            return true;
        }
        std::vector<std::string> actor_digests_;
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_ACRTOR_H
