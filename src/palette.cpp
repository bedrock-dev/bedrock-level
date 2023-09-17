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
    std::tuple<abstract_tag *, size_t> read_nbt(const byte_t *data);
    std::tuple<compound_tag *, size_t> read_compound_value(const byte_t *data,
                                                           const std::string &key);

    int read_string(const byte_t *data, std::string &val) {
        uint16_t len = *reinterpret_cast<const uint16_t *>(data);
        if (len != 0) {
            val = std::string(data + 2, data + len + 2);
        }
        return len + 2;
    }

    int read_tag_type(const byte_t *data, tag_type &type) {
        type = static_cast<tag_type>(data[0]);
        return 1;
    }

    std::tuple<byte_array_tag *, size_t> read_byte_array_value(const byte_t *data,
                                                               const std::string &key) {
        auto *tag = new byte_array_tag(key);
        int32_t len = {0};
        memcpy(&len, data, 4);
        tag->value = std::vector<int8_t>(len, 0);
        memcpy(tag->value.data(), data + 4, len);
        return {tag, len * 1 + 4};
    }

    std::tuple<int_array_tag *, size_t> read_int_array_value(const byte_t *data,
                                                             const std::string &key) {
        auto *tag = new int_array_tag(key);
        int32_t len{0};
        memcpy(&len, data, 4);
        tag->value = std::vector<int32_t>(len, 0);
        memcpy(tag->value.data(), data + 4, len * 4);
        return {tag, len * 4 + 4};
    }

    std::tuple<long_array_tag *, size_t> read_long_array_value(const byte_t *data,
                                                               const std::string &key) {
        auto *tag = new long_array_tag(key);
        int32_t len{0};
        memcpy(&len, data, 4);
        BL_LOGGER("Len is %d", len);
        tag->value = std::vector<int64_t>(len, 0);
        memcpy(tag->value.data(), data + 4, len * 8);
        return {tag, len * 8 + 4};
    }

    std::tuple<list_tag *, size_t> read_list_tag_value(const byte_t *data, const std::string &key) {
        size_t read = 0;
        auto *tag = new list_tag(key);
        tag_type child_type;
        read += read_tag_type(data + read, child_type);
        int32_t list_size{0};
        memcpy(&list_size, data + read, 4);
        read += 4;
        for (int i = 0; i < list_size; i++) {
            if (child_type == Int) {
                auto *child = new int_tag("");
                memcpy(&child->value, data + read, 4);
                tag->value.push_back(child);
                read += 4;
            } else if (child_type == Short) {
                auto *child = new short_tag("");
                memcpy(&child->value, data + read, 2);
                tag->value.push_back(child);
                read += 2;
            } else if (child_type == Long) {
                auto *child = new long_tag("");
                memcpy(&child->value, data + read, 8);
                tag->value.push_back(child);
                read += 8;
            } else if (child_type == Float) {
                auto *child = new float_tag("");
                memcpy(&child->value, data + read, 4);
                tag->value.push_back(child);
                read += 4;
            } else if (child_type == Double) {
                auto *child = new double_tag("");
                memcpy(&child->value, data + read, 8);
                tag->value.push_back(child);
                read += 8;
            } else if (child_type == String) {
                auto *child = new string_tag("");
                read += read_string(data + read, child->value);
                tag->value.push_back(child);
            } else if (child_type == ByteArray) {
                auto [t, sz] = read_byte_array_value(data + read, "");
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == IntArray) {
                auto [t, sz] = read_int_array_value(data + read, "");
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == LongArray) {
                auto [t, sz] = read_long_array_value(data + read, "");
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == Compound) {
                auto [t, sz] = read_compound_value(data + read, "");
                read += sz;
                tag->value.push_back(t);
            } else if (child_type == List) {
                auto [t, sz] = read_list_tag_value(data + read, "");
                read += sz;
                tag->value.push_back(t);
            } else {
                throw std::runtime_error("unsupported list child tag type " +
                                         std::to_string((int)child_type));
            }
        }
        return {tag, read};
    }

    std::tuple<compound_tag *, size_t> read_compound_value(const byte_t *data,
                                                           const std::string &key) {
        auto *tag = new compound_tag(key);
        size_t total = 0;
        while (true) {
            auto [child, read] = read_nbt(data + total);
            total += read;
            if (child) {
                tag->value[child->key()] = child;
            } else {
                break;
            }
        }
        return {tag, total};
    }

    /*
     * 不保证内存够用
     *
     */
    std::tuple<abstract_tag *, size_t> read_nbt(const byte_t *data) {
        int read = 0;
        tag_type type;
        read += read_tag_type(data, type);
        if (type == End) {
            return {nullptr, 1};
        }
        std::string key;
        read += read_string(data + read, key);
        if (type == Compound) {
            auto [res, len] = read_compound_value(data + read, key);
            return {res, read + len};
        } else if (type == Int) {
            auto *tag = new int_tag(key);
            memcpy(&tag->value, data + read, 4);
            return {tag, 4 + read};
        } else if (type == Short) {
            auto *tag = new short_tag(key);
            memcpy(&tag->value, data + read, 2);
            return {tag, 2 + read};
        } else if (type == Long) {
            auto *tag = new long_tag(key);
            memcpy(&tag->value, data + read, 8);
            return {tag, 8 + read};
        } else if (type == Float) {
            auto *tag = new float_tag(key);
            memcpy(&tag->value, data + read, 4);
            return {tag, 4 + read};
        } else if (type == Double) {
            auto *tag = new double_tag(key);
            memcpy(&tag->value, data + read, 8);
            return {tag, 8 + read};
        } else if (type == Byte) {
            auto *tag = new byte_tag(key);
            memcpy(&tag->value, data + read, 1);
            return {tag, 1 + read};
        } else if (type == String) {
            auto *tag = new string_tag(key);
            auto len = read_string(data + read, tag->value);
            return {tag, len + read};
        } else if (type == ByteArray) {
            auto [res, len] = read_byte_array_value(data + read, key);
            return {res, len + read};
        } else if (type == IntArray) {
            auto [res, len] = read_int_array_value(data + read, key);
            return {res, len + read};
        } else if (type == LongArray) {
            BL_LOGGER("key size(%d)", key.size());
            auto [res, len] = read_long_array_value(data + read, key);
            return {res, len + read};
        } else if (type == List) {
            auto [res, len] = read_list_tag_value(data + read, key);
            return {res, len + read};
        } else {
            throw std::runtime_error("unsupported tag type " + std::to_string((int)type));
        }
    }

    [[maybe_unused]] compound_tag *read_one_palette(const byte_t *data, int &read) {
        read = 0;
        auto [r, x] = read_nbt(data);
        read = static_cast<int>(x);
        if (!r || r->type() != tag_type::Compound) {
            BL_ERROR("Invalid palette format");
            delete r;
            return nullptr;
        } else {
            return dynamic_cast<compound_tag *>(r);
        }
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
            res.push_back(read_one_palette(data + ptr, read));
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
