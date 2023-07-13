//
// Created by xhy on 2023/7/13.
//

#ifndef BEDROCK_LEVEL_NBT_H
#define BEDROCK_LEVEL_NBT_H
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"
namespace bl::nbt {
    enum tag_type : int8_t {
        End = 0,
        Byte = 1,
        Short = 2,
        Int = 3,
        Long = 4,
        Float = 5,
        Double = 6,
        String = 8,
        List = 9,
        Compound = 10,
        LEN = 11
    };

    struct abstract_tag {};

    template <tag_type T>
    struct tag_mapper;

#define ADD_MAPPER(TAG_TYPE, VALUE_TYPE) \
    template <>                          \
    struct tag_mapper<TAG_TYPE> {        \
        typedef VALUE_TYPE value_type;   \
    }

    ADD_MAPPER(Int, int32_t);
    ADD_MAPPER(Byte, int8_t);
    ADD_MAPPER(Short, int16_t);
    ADD_MAPPER(Long, int32_t);
    ADD_MAPPER(Float, float);
    ADD_MAPPER(Double, double);
    ADD_MAPPER(String, std::string);
    ADD_MAPPER(List, std::vector<abstract_tag *>);
    typedef std::map<std::string, abstract_tag *> compound_tag_type;
    ADD_MAPPER(Compound, compound_tag_type);

    template <tag_type T>
    class tag : public abstract_tag {
        std::string key() { return this->key_; }
        tag_type type() { return T; }

       private:
        std::string key_;
        typename tag_mapper<T>::value_type value_;
    };

    abstract_tag *read_nbt(const byte_t *, int len);

}  // namespace bl::nbt

#endif  // BEDROCK_LEVEL_NBT_H
