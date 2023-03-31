//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include "utils.h"

namespace bl::palette {

    namespace {
        int read_string(const uint8_t *data, std::string &val) {
            uint16_t len = *reinterpret_cast<const uint16_t *>(data);
            if (len != 0) {
                val = std::string(data + 2, data + len + 2);
            }

            return len + 2;
        }

        int read_int(const uint8_t *data, int32_t &val) {
            val = *reinterpret_cast<const int32_t *>(data);
            return 4;
        }

        int read_byte(const uint8_t *data, uint8_t &val) {
            val = data[0];
            return 1;
        }

        int read_type(const uint8_t *data, tag_type &type) {
            type = static_cast<tag_type>(data[0]);
            return 1;
        }

        abstract_tag *read_nbt(const uint8_t *data, int &read) {
            read = 0;
            tag_type type;
            read += read_type(data, type);
            if (type == End) {
                return nullptr;
            }
            std::string key;
            read += read_string(data + read, key);
            if (type == Compound) {
                auto *tag = new compound_tag(key);
                abstract_tag *child;
                do {
                    int r;
                    child = read_nbt(data + read, r);
                    if (child) {
                        tag->value[child->key()] = child;
                    }
                    read += r;
                } while (child);
                return tag;
            } else if (type == Int) {
                auto *tag = new int_tag(key);
                read += read_int(data + read, tag->value);
                return tag;
            } else if (type == String) {
                auto *tag = new string_tag(key);
                read += read_string(data + read, tag->value);
                return tag;
            } else if (type == Byte) {
                auto *tag = new byte_tag(key);
                read += read_byte(data + read, tag->value);
                return tag;
            } else {
                throw std::runtime_error("unsupported tag type " + std::to_string((int)type));
            }
        }
    }  // namespace

    [[maybe_unused]] compound_tag *read_one_palette(const uint8_t *data, int &read) {
        read = 0;
        auto r = read_nbt(data, read);
        if (!r || r->type() != tag_type::Compound) {
            BL_ERROR("Invalid palette format");
        } else {
            return dynamic_cast<compound_tag *>(r);
        }
        return nullptr;
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
            case LEN:
                return "ERROR";
        }
        return "ERROR";
    }

}  // namespace bl::palette
