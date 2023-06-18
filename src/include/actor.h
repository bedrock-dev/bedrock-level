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

        void dump();
        [[nodiscard]] vec3 pos() const { return this->pos_; };
        [[nodiscard]] std::string identifier() const { return this->identifier_; };
        actor() = default;

       private:
        bool loaded = false;
        bl::palette::compound_tag* root_{nullptr};
        std::string identifier_{"minecraft:unknown"};
        vec3 pos_{0, 0, 0};
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_ACRTOR_H
