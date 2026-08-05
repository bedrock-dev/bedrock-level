//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include <cstdlib>
#include <cstring>

#include "magic-enum/magic_enum.hpp"
#include "utils.h"

namespace bl::palette {

    std::tuple<abstract_tag *, size_t> read_nbt(const byte_t *data, size_t data_len);
    std::tuple<compound_tag *, size_t> read_compound_value(const byte_t *data, size_t data_len, const std::string &key);
    std::tuple<abstract_tag *, size_t> read_value_by_type(tag_type type, const byte_t *data, size_t data_len, const std::string &key);

    std::string tag_type_to_str(tag_type type) {
        auto name = magic_enum::enum_name(type);
        return name.empty() ? "UNKNOWN" : std::string(name);
    }

    // used by all key and string value
    int read_string(const byte_t *data, size_t data_len, std::string &val) {
        if (data_len < 2) return 0;
        uint16_t len;
        memcpy(&len, data, 2);
        if (data_len < 2u + len) return 0;
        if (len != 0) {
            val.assign(data + 2, len);
        } else {
            val.clear();
        }
        return len + 2;
    }

    int read_tag_type(const byte_t *data, size_t data_len, tag_type &type) {
        if (data_len < 1) return 0;
        type = static_cast<tag_type>(data[0]);
        return 1;
    }

    template <typename TagType>
    std::tuple<TagType *, size_t> read_scalar_value(const byte_t *data, size_t data_len, const std::string &key) {
        using value_type = decltype(TagType::value);
        constexpr size_t value_size = sizeof(value_type);
        if (data_len < value_size) return {nullptr, 0};
        auto *tag = new TagType(key);
        memcpy(&tag->value, data, value_size);
        return {tag, value_size};
    }

    template <typename TagType>
    std::tuple<TagType *, size_t> read_array_value(const byte_t *data, size_t data_len, const std::string &key) {
        using elem_type = typename decltype(TagType::value)::value_type;
        if (data_len < 4) return {nullptr, 0};
        int32_t len = 0;
        memcpy(&len, data, 4);
        if (data_len < 4u + (size_t)len * sizeof(elem_type)) return {nullptr, 0};
        auto *tag = new TagType(key);
        tag->value = std::vector<elem_type>(len, 0);
        memcpy(tag->value.data(), data + 4, len * sizeof(elem_type));
        return {tag, len * sizeof(elem_type) + 4};
    }

    std::tuple<string_tag *, size_t> read_string_value(const byte_t *data, size_t data_len, const std::string &key) {
        auto *tag = new string_tag(key);
        int r = read_string(data, data_len, tag->value);
        if (r == 0) {
            delete tag;
            return {nullptr, 0};
        }
        return {tag, static_cast<size_t>(r)};
    }

    std::tuple<list_tag *, size_t> read_list_tag_value(const byte_t *data, size_t data_len, const std::string &key) {
        size_t read = 0;
        auto *tag = new list_tag(key);
        tag_type child_type;
        {
            int r = read_tag_type(data + read, data_len - read, child_type);
            if (r == 0) return {tag, read};
            read += r;
        }
        if (data_len - read < 4) return {tag, read};
        int32_t list_size{0};
        memcpy(&list_size, data + read, 4);
        read += 4;
        tag->value.reserve(std::min<size_t>(static_cast<size_t>(list_size), data_len - read));
        for (int i = 0; i < list_size; i++) {
            if (data_len <= read) break;
            auto [child, sz] = read_value_by_type(child_type, data + read, data_len - read, "");
            if (child == nullptr) break;
            read += sz;
            tag->value.push_back(child);
        }
        return {tag, read};
    }

    std::tuple<compound_tag *, size_t> read_compound_value(const byte_t *data, size_t data_len, const std::string &key) {
        auto *tag = new compound_tag(key);
        size_t total = 0;
        while (total < data_len) {
            auto [child, read] = read_nbt(data + total, data_len - total);
            if (read == 0) break;
            total += read;
            if (child) {
                tag->value.assign(child);
            } else {
                break;
            }
        }
        return {tag, total};
    }

    std::tuple<abstract_tag *, size_t> read_value_by_type(tag_type type, const byte_t *data, size_t data_len, const std::string &key) {
        switch (type) {
            case Compound:
                return read_compound_value(data, data_len, key);
            case Int:
                return read_scalar_value<int_tag>(data, data_len, key);
            case Short:
                return read_scalar_value<short_tag>(data, data_len, key);
            case Long:
                return read_scalar_value<long_tag>(data, data_len, key);
            case Float:
                return read_scalar_value<float_tag>(data, data_len, key);
            case Double:
                return read_scalar_value<double_tag>(data, data_len, key);
            case Byte:
                return read_scalar_value<byte_tag>(data, data_len, key);
            case String:
                return read_string_value(data, data_len, key);
            case ByteArray:
                return read_array_value<byte_array_tag>(data, data_len, key);
            case IntArray:
                return read_array_value<int_array_tag>(data, data_len, key);
            case LongArray:
                return read_array_value<long_array_tag>(data, data_len, key);
            case List:
                return read_list_tag_value(data, data_len, key);
            default:
                throw std::runtime_error("unsupported tag type " + std::to_string((int)type));
        }
    }

    std::tuple<abstract_tag *, size_t> read_nbt(const byte_t *data, size_t data_len) {
        if (data_len == 0) return {nullptr, 0};
        int read = 0;
        tag_type type;
        {
            int r = read_tag_type(data, data_len, type);
            if (r == 0) return {nullptr, 0};
            read += r;
        }
        if (type == End) {
            return {nullptr, 1};
        }
        std::string key;
        {
            int r = read_string(data + read, data_len - read, key);
            if (r == 0) return {nullptr, 0};
            read += r;
        }
        auto [tag, len] = read_value_by_type(type, data + read, data_len - read, key);
        if (tag == nullptr) return {nullptr, 0};
        return {tag, read + len};
    }

    compound_tag *read_one_palette(const byte_t *data, int &read) {
        // legacy overload: no bounds, caller must ensure enough data
        return read_one_palette(data, SIZE_MAX, read);
    }

    compound_tag *read_one_palette(const byte_t *data, size_t data_len, int &read) {
        read = 0;
        auto [r, x] = read_nbt(data, data_len);
        read = static_cast<int>(x);
        if (!r || r->type() != tag_type::Compound) {
            BL_ERROR("Invalid palette format");
            delete r;
            return nullptr;
        } else {
            return r->as<compound_tag *>();
        }
    }

    std::vector<compound_tag *> read_palette_to_end(const byte_t *data, size_t len) {
        size_t ptr = 0;
        std::vector<compound_tag *> res;
        while (ptr < len) {
            int read;
            auto *tag = read_one_palette(data + ptr, len - ptr, read);
            if (read == 0) break;
            ptr += read;
            if (tag) res.push_back(tag);
        }
        if (ptr != len) {
            BL_ERROR("Remain bytes found (%d).", (int)len - (int)ptr);
        }
        return res;
    }
    list_tag::~list_tag() {
        for (auto tag : this->value) {
            delete tag;
        }
    }
}  // namespace bl::palette
