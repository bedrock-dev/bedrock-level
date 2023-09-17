//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <cstdlib>
#include <cstring>
#include <map>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils.h"

namespace bl::palette {

    // TODD: 使用模板重写这个库
    enum tag_type : int8_t {
        End = 0,
        Byte = 1,
        Short = 2,
        Int = 3,
        Long = 4,
        Float = 5,
        Double = 6,
        ByteArray = 7,
        String = 8,
        List = 9,
        Compound = 10,
        IntArray = 11,
        LongArray = 12
    };
    std::string tag_type_to_str(tag_type type);

    class abstract_tag {
       public:
        explicit abstract_tag(std::string key) : key_(std::move(key)) {}

        abstract_tag(const abstract_tag &tag) = default;

        [[nodiscard]] std::string to_readable_string() const {
            std::stringstream s;
            this->write(s, 0);
            return s.str();
        }

        abstract_tag &operator=(const abstract_tag &tag) = default;

       public:
        [[nodiscard]] virtual tag_type type() const = 0;

        [[nodiscard]] virtual std::string value_string() const = 0;
        [[nodiscard]] virtual abstract_tag *copy() const = 0;

        /**
         *
         * 这个函数有bug，暂时不要使用
         * @return
         */
        [[nodiscard]] virtual std::string to_raw() const {
            return this->type_to_raw() + this->key_to_raw() + this->payload_to_raw();
        }

        [[nodiscard]] std::string key() const { return this->key_; }
        void set_key(const std::string &key) { this->key_ = key; }

        [[nodiscard]] virtual std::string payload_to_raw() const = 0;

       public:
        virtual void write(std::ostream &o, int indent) const {
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << tag_type_to_str(this->type()) << "('" << this->key_ << "'): ";
        }

       public:
        virtual ~abstract_tag() = default;

       protected:
        [[nodiscard]] std::string key_to_raw() const {
            std::string res(2, '0');
            auto size = static_cast<uint16_t>(this->key_.size());
            memcpy(res.data(), &size, 2);
            return res + this->key_;
        }

        [[nodiscard]] std::string type_to_raw() const {
            std::string res;
            res.push_back(static_cast<char>(type()));
            return res;
        }

        std::string key_;
    };

    struct compound_tag : public abstract_tag {
        explicit compound_tag(const std::string &key) : abstract_tag(key) {}
        compound_tag(const compound_tag &tag) : abstract_tag(tag.key_) {
            this->key_ = tag.key_;
            for (auto &kv : tag.value) {
                this->value[kv.first] = kv.second->copy();
            }
        }

        compound_tag &operator=(const compound_tag &tag) {
            this->key_ = tag.key_;
            for (auto &kv : tag.value) {
                this->value[kv.first] = kv.second->copy();
            }
            return *this;
        }

        [[nodiscard]] tag_type type() const override { return Compound; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "{\n";
            for (auto &kv : this->value) {
                kv.second->write(o, indent + 4);
            }

            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << "}\n";
        }

        [[nodiscard]] std::string value_string() const override { return "(...)"; };

        void put(abstract_tag *tag) {
            auto it = this->value.find(tag->key());
            if (it != this->value.end()) {
                delete it->second;
            }
            this->value[tag->key()] = tag;
        }

        void remove(const std::string &key) {
            auto it = this->value.find(key);
            if (it != this->value.end()) {
                delete it->second;
            }
            this->value.erase(key);
        }

        abstract_tag *get(const std::string &key) {
            auto it = this->value.find(key);
            return it == this->value.end() ? nullptr : it->second;
        }

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new compound_tag(this->key_);
            for (auto &kv : this->value) {
                res->put(kv.second->copy());
            }
            return res;
        }

        ~compound_tag() override {
            for (auto &kv : this->value) {
                delete kv.second;
            }
        }

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string res;
            for (auto &kv : this->value) {
                res += kv.second->to_raw();
            }
            res += std::string(1, static_cast<char>(bl::palette::tag_type::End));
            return res;
        }

       public:
        std::map<std::string, abstract_tag *> value;
    };

    struct string_tag : public abstract_tag {
        explicit string_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return String; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "'" << this->value << "'" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override { return this->value; };

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new string_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~string_tag() override = default;
        std::string value;

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string res(2, '\0');
            auto len = static_cast<uint16_t>(this->value.size());
            memcpy(res.data(), &len, 2);
            return res + this->value;
        }
    };

    struct int_tag : public abstract_tag {
        explicit int_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Int; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        };
        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new int_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~int_tag() override = default;
        int32_t value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 4;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct short_tag : public abstract_tag {
        explicit short_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Short; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        ~short_tag() override = default;

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new short_tag(this->key_);
            res->value = this->value;
            return res;
        }

        int16_t value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 2;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct long_tag : public abstract_tag {
        explicit long_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Long; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        };
        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new long_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~long_tag() override = default;

        int64_t value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 8;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct float_tag : public abstract_tag {
        explicit float_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Float; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new float_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~float_tag() override = default;
        float value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 4;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct double_tag : public abstract_tag {
        explicit double_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Double; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new double_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~double_tag() override = default;

        double value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 8;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct byte_tag : public abstract_tag {
        explicit byte_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Byte; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << static_cast<int>(this->value) << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new byte_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~byte_tag() override = default;
        int8_t value{};

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            const size_t N = 1;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return res;
        }
    };

    struct byte_array_tag : public abstract_tag {
        explicit byte_array_tag(const std::string &key) : abstract_tag(key) {}

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[ ..." << this->value.size() << " X 1 bytes ... ]" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return "[ ..." + std::to_string(this->value.size()) + "... ]";
        }
        [[nodiscard]] tag_type type() const override { return ByteArray; }

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new byte_array_tag(this->key_);
            res->value = this->value;
            return res;
        }
        ~byte_array_tag() override = default;
        std::vector<int8_t> value;

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string raw(4 + this->value.size() * 1, 0);
            auto size = static_cast<int32_t>(this->value.size());
            memcpy(raw.data(), &size, 4);
            memcpy(raw.data() + 4, this->value.data(), this->value.size());
            return raw;
        }
    };

    struct int_array_tag : public abstract_tag {
        explicit int_array_tag(const std::string &key) : abstract_tag(key) {}

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[ ..." << this->value.size() << " X 4 bytes ... ]" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return "[ ..." + std::to_string(this->value.size()) + "... ]";
        }
        [[nodiscard]] tag_type type() const override { return ByteArray; }

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new int_array_tag(this->key_);
            res->value = this->value;
            return res;
        }
        ~int_array_tag() override = default;
        std::vector<int32_t> value;

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string raw(4 + this->value.size() * 4, 0);
            auto size = static_cast<int32_t>(this->value.size());
            memcpy(raw.data(), &size, 4);
            memcpy(raw.data() + 4, this->value.data(), this->value.size() * 4);
            return raw;
        }
    };

    struct long_array_tag : public abstract_tag {
        explicit long_array_tag(const std::string &key) : abstract_tag(key) {}

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[ ..." << this->value.size() << " X 8 bytes ... ]" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return "[ ..." + std::to_string(this->value.size()) + "... ]";
        }
        [[nodiscard]] tag_type type() const override { return ByteArray; }

        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new long_array_tag(this->key_);
            res->value = this->value;
            return res;
        }
        ~long_array_tag() override = default;
        std::vector<int64_t> value;

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string raw(4 + this->value.size() * 8, 0);
            auto size = static_cast<int32_t>(this->value.size());
            memcpy(raw.data(), &size, 4);

            memcpy(raw.data() + 4, this->value.data(), this->value.size() * 8);
            return raw;
        }
    };

    struct list_tag : public abstract_tag {
        friend class abstract_tag;

        list_tag(const list_tag &tag) : abstract_tag(tag.key_) {
            for (auto &k : tag.value) {
                this->value.push_back(k->copy());
            }
        }
        list_tag &operator=(const list_tag &tag) {
            this->key_ = tag.key_;
            for (auto &k : tag.value) {
                this->value.push_back(k->copy());
            }
            return *this;
        }
        explicit list_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return List; }

        void write(std::ostream &o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[" << this->value.size() << "] ";
            o << "{\n";
            for (auto &tag : this->value) {
                tag->write(o, indent + 4);
            }
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << "}\n";
        }
        [[nodiscard]] abstract_tag *copy() const override {
            auto *res = new list_tag(this->key_);
            for (auto &item : this->value) {
                res->value.push_back(item->copy());
            }

            return res;
        }

        [[nodiscard]] std::string value_string() const override { return "[...]"; };
        void append(abstract_tag *tag) {
            if (tag) {
                this->value.push_back(tag);
            }
        }

        ~list_tag() override;
        std::vector<abstract_tag *> value;

       protected:
        [[nodiscard]] std::string payload_to_raw() const override {
            std::string res(5, 0);
            auto child_type = End;
            if (!value.empty()) {
                // assume on nullptr in list
                child_type = value[0]->type();
            }
            res[0] = static_cast<char>(child_type);
            auto sz = static_cast<int32_t>(value.size());
            memcpy(res.data() + 1, &sz, 4);
            for (auto &child : value) {
                res += child->payload_to_raw();
            }
            return res;
        }
    };

    compound_tag *read_one_palette(const byte_t *data, int &read);

    std::vector<compound_tag *> read_palette_to_end(const byte_t *data, size_t len);
}  // namespace bl::palette

#endif  // BEDROCK_LEVEL_PALETTE_H
