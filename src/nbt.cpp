//
// Created by xhy on 2023/7/13.
//

#include "nbt.h"

#include <tuple>
namespace bl::nbt {

    //    template <tag_type T>
    //    std::tuple<tag<T> *, size_t> read_one_tag(const byte_t *, int len);

    template <tag_type T>
    struct tag_reader;

    std::tuple<tag_type, size_t> read_tag_type(const byte_t *data) {
        auto type = static_cast<tag_type>(data[0]);
        return {type, 1};
    }
    std::tuple<std::string, size_t> read_tag_key(const byte_t *data) {
        uint16_t len = *reinterpret_cast<const uint16_t *>(data);
        std::string res;
        if (len != 0) res = std::string(data + 2, data + len + 2);
        return {res, len + 2};
    }

    abstract_tag *read_nbt(const byte_t *data, int &len) {
        auto [type, type_len] = read_tag_type(data);
        if (type == End) return nullptr;
        auto [key, key_len] = read_tag_key(data + type_len);
        abstract_tag *r{nullptr};

        return nullptr;
    }
}  // namespace bl::nbt
