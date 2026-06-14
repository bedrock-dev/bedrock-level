//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include <cstdlib>
#include <cstring>

#include "utils.h"

namespace bl::palette {

    /**
     * 从数据流中读取一个nbt，可以是任何类型，返回指针和读取的长度
     * @param data
     * @return
     */
    std::tuple<abstract_tag *, size_t> read_nbt(const byte_t *data, size_t data_len);
    std::tuple<compound_tag *, size_t> read_compound_value(const byte_t *data, size_t data_len, const std::string &key);

    int read_string(const byte_t *data, size_t data_len, std::string &val) {
        if (data_len < 2) return 0;
        uint16_t len;
        memcpy(&len, data, 2);
        if (data_len < 2u + len) return 0;
        if (len != 0) {
            val.assign(data + 2, len);
        }
        return len + 2;
    }

    int read_tag_type(const byte_t *data, size_t data_len, tag_type &type) {
        if (data_len < 1) return 0;
        type = static_cast<tag_type>(data[0]);
        return 1;
    }

    template <typename TagType, typename ValueType, size_t ValueSize>
    std::tuple<TagType *, size_t> read_scalar_value(const byte_t *data, size_t data_len, const std::string &key) {
        if (data_len < ValueSize) return {nullptr, 0};
        auto *tag = new TagType(key);
        memcpy(&tag->value, data, ValueSize);
        return {tag, ValueSize};
    }

    template <typename TagType, typename ElemType>
    std::tuple<TagType *, size_t> read_array_value(const byte_t *data, size_t data_len, const std::string &key) {
        if (data_len < 4) return {nullptr, 0};
        int32_t len = 0;
        memcpy(&len, data, 4);
        if (data_len < 4u + (size_t)len * sizeof(ElemType)) return {nullptr, 0};
        auto *tag = new TagType(key);
        tag->value = std::vector<ElemType>(len, 0);
        memcpy(tag->value.data(), data + 4, len * sizeof(ElemType));
        return {tag, len * sizeof(ElemType) + 4};
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
        for (int i = 0; i < list_size; i++) {
            if (data_len <= read) break;
            if (child_type == Int) {
                auto [child, sz] = read_scalar_value<int_tag, int32_t, 4>(data + read, data_len - read, "");
                if (child == nullptr) break;
                read += sz;
                tag->value.push_back(child);
            } else if (child_type == Short) {
                auto [child, sz] = read_scalar_value<short_tag, int16_t, 2>(data + read, data_len - read, "");
                if (child == nullptr) break;
                read += sz;
                tag->value.push_back(child);
            } else if (child_type == Long) {
                auto [child, sz] = read_scalar_value<long_tag, int64_t, 8>(data + read, data_len - read, "");
                if (child == nullptr) break;
                read += sz;
                tag->value.push_back(child);
            } else if (child_type == Float) {
                auto [child, sz] = read_scalar_value<float_tag, float, 4>(data + read, data_len - read, "");
                if (child == nullptr) break;
                read += sz;
                tag->value.push_back(child);
            } else if (child_type == Double) {
                auto [child, sz] = read_scalar_value<double_tag, double, 8>(data + read, data_len - read, "");
                if (child == nullptr) break;
                read += sz;
                tag->value.push_back(child);
            } else if (child_type == String) {
                auto *child = new string_tag("");
                int r = read_string(data + read, data_len - read, child->value);
                if (r == 0) break;
                read += r;
                tag->value.push_back(child);
            } else if (child_type == ByteArray) {
                auto [t, sz] = read_array_value<byte_array_tag, int8_t>(data + read, data_len - read, "");
                if (t == nullptr) break;
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == IntArray) {
                auto [t, sz] = read_array_value<int_array_tag, int32_t>(data + read, data_len - read, "");
                if (t == nullptr) break;
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == LongArray) {
                auto [t, sz] = read_array_value<long_array_tag, int64_t>(data + read, data_len - read, "");
                if (t == nullptr) break;
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == Compound) {
                auto [t, sz] = read_compound_value(data + read, data_len - read, "");
                if (t == nullptr) break;
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == List) {
                auto [t, sz] = read_list_tag_value(data + read, data_len - read, "");
                if (t == nullptr) break;
                read += sz;
                tag->value.push_back(t);
            } else {
                throw std::runtime_error("unsupported list child tag type " + std::to_string((int)child_type));
            }
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
                tag->value[child->key()] = child;
            } else {
                break;
            }
        }
        return {tag, total};
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
        if (type == Compound) {
            auto [res, len] = read_compound_value(data + read, data_len - read, key);
            return {res, read + len};
        } else if (type == Int) {
            auto [tag, len] = read_scalar_value<int_tag, int32_t, 4>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == Short) {
            auto [tag, len] = read_scalar_value<short_tag, int16_t, 2>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == Long) {
            auto [tag, len] = read_scalar_value<long_tag, int64_t, 8>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == Float) {
            auto [tag, len] = read_scalar_value<float_tag, float, 4>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == Double) {
            auto [tag, len] = read_scalar_value<double_tag, double, 8>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == Byte) {
            auto [tag, len] = read_scalar_value<byte_tag, int8_t, 1>(data + read, data_len - read, key);
            if (tag == nullptr) return {nullptr, 0};
            return {tag, read + len};
        } else if (type == String) {
            auto *tag = new string_tag(key);
            int r = read_string(data + read, data_len - read, tag->value);
            if (r == 0) {
                delete tag;
                return {nullptr, 0};
            }
            return {tag, r + read};
        } else if (type == ByteArray) {
            auto [res, len] = read_array_value<byte_array_tag, int8_t>(data + read, data_len - read, key);
            if (res == nullptr) return {nullptr, 0};
            return {res, len + read};
        } else if (type == IntArray) {
            auto [res, len] = read_array_value<int_array_tag, int32_t>(data + read, data_len - read, key);
            if (res == nullptr) return {nullptr, 0};
            return {res, len + read};
        } else if (type == LongArray) {
            auto [res, len] = read_array_value<long_array_tag, int64_t>(data + read, data_len - read, key);
            if (res == nullptr) return {nullptr, 0};
            return {res, len + read};
        } else if (type == List) {
            auto [res, len] = read_list_tag_value(data + read, data_len - read, key);
            if (res == nullptr) return {nullptr, 0};
            return {res, len + read};
        } else {
            throw std::runtime_error("unsupported tag type " + std::to_string((int)type));
        }
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
            return dynamic_cast<compound_tag *>(r);
        }
    }

    compound_tag *read_one_palette(const byte_t *data, int &read) {
        // legacy overload: no bounds, caller must ensure enough data
        return read_one_palette(data, SIZE_MAX, read);
    }

    std::string tag_type_to_str(tag_type type) {
        switch (type) {
            case End:
                return "End";
            case Byte:
                return "Byte";
            case Int:
                return "Int";
            case String:
                return "String";
            case Compound:
                return "Compound";
            case List:
                return "List";
            case Long:
                return "Long";
                break;
            case Float:
                return "Float";
            case Double:
                return "Double";
            case Short:
                return "Short";
            case ByteArray:
                return "ByteArray";
            case IntArray:
                return "IntArray";
            case LongArray:
                return "LongArray";
        }
        return "UNKNOWN";
    }

    std::vector<compound_tag *> read_palette_to_end(const byte_t *data, size_t len) {
        size_t ptr = 0;
        std::vector<compound_tag *> res;
        while (ptr < len) {
            int read;
            res.push_back(read_one_palette(data + ptr, len - ptr, read));
            if (read == 0) break;
            ptr += read;
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
