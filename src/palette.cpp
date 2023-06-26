//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include "utils.h"

namespace bl::palette {

    namespace {
        int read_string(const byte_t *data, std::string &val) {
            uint16_t len = *reinterpret_cast<const uint16_t *>(data);
            if (len != 0) {
                val = std::string(data + 2, data + len + 2);
            }
            return len + 2;
        }

        int read_int(const byte_t *data, int32_t &val) {
            val = *reinterpret_cast<const int32_t *>(data);
            return 4;
        }

        int read_short(const byte_t *data, int16_t &val) {
            val = *reinterpret_cast<const int16_t *>(data);
            return 2;
        }

        int read_byte(const byte_t *data, uint8_t &val) {
            val = data[0];
            return 1;
        }

        int read_long(const byte_t *data, int64_t &val) {
            val = *reinterpret_cast<const int64_t *>(data);
            return 8;
        }

        int read_float(const byte_t *data, float &val) {
            val = *reinterpret_cast<const float *>(data);
            return 4;
        }

        int read_double(const byte_t *data, double &val) {
            val = *reinterpret_cast<const double *>(data);
            return 8;
        }

        int read_type(const byte_t *data, tag_type &type) {
            type = static_cast<tag_type>(data[0]);
            return 1;
        }

        /*
         * 不保证内存够用
         */
        abstract_tag *read_nbt(const byte_t *data, int &read) {
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
            } else if (type == Short) {
                auto *tag = new short_tag(key);
                read += read_short(data + read, tag->value);
                return tag;
            } else if (type == Long) {
                auto *tag = new long_tag(key);
                read += read_long(data + read, tag->value);
                return tag;
            } else if (type == Float) {
                auto *tag = new float_tag(key);
                read += read_float(data + read, tag->value);
                return tag;
            } else if (type == Double) {
                auto *tag = new double_tag(key);
                read += read_double(data + read, tag->value);
                return tag;
            } else if (type == String) {
                auto *tag = new string_tag(key);
                read += read_string(data + read, tag->value);
                return tag;
            } else if (type == Byte) {
                auto *tag = new byte_tag(key);
                read += read_byte(data + read, tag->value);
                return tag;
            } else if (type == List) {
                auto *tag = new list_tag(key);
                tag_type child_type;
                read += read_type(data + read, child_type);
                read += read_int(data + read, tag->size);
                for (int i = 0; i < tag->size; i++) {
                    if (child_type == Int) {
                        auto *child = new int_tag("");
                        read += read_int(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == Short) {
                        auto *child = new short_tag("");
                        read += read_short(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == Long) {
                        auto *child = new long_tag("");
                        read += read_long(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == Float) {
                        auto *child = new float_tag("");
                        read += read_float(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == Double) {
                        auto *child = new double_tag("");
                        read += read_double(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == String) {
                        auto *child = new string_tag("");
                        read += read_string(data + read, child->value);
                        tag->value.push_back(child);
                    } else if (child_type == Compound) {
                        auto *child = new compound_tag("");
                        abstract_tag *cc;
                        do {
                            int r;
                            cc = read_nbt(data + read, r);
                            if (cc) {
                                child->value[cc->key()] = cc;
                            }
                            read += r;
                        } while (cc);
                        tag->value.push_back(child);
                    } else {
                        throw std::runtime_error("unsupported list child tag type " +
                                                 std::to_string((int)child_type));
                    }
                }
                return tag;
            } else {
                throw std::runtime_error("unsupported tag type " + std::to_string((int)type));
            }
        }
    }  // namespace

    [[maybe_unused]] compound_tag *read_one_palette(const byte_t *data, int &read) {
        read = 0;
        auto r = read_nbt(data, read);
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
            case LEN:
                return "ERROR";
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
                break;
        }
        return "UNKNOWN";
    }

    std::vector<compound_tag *> read_palette_to_end(const byte_t *data, size_t len) {
        size_t ptr = 0;
        std::vector<compound_tag *> res;
        while (ptr < len) {
            int read;
            res.push_back(read_one_palette(data, read));
            ptr += read;
        }
        if (ptr != len) {
            BL_ERROR("Remain bytes found.");
        }
        return res;
    }

    compound_tag::~compound_tag() {
        for (auto &kv : this->value) {
            delete kv.second;
        }
    }
    list_tag::~list_tag() {
        for (auto tag : this->value) {
            delete tag;
        }
    }
}  // namespace bl::palette
