//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_PALETTE_H
#define BEDROCK_LEVEL_PALETTE_H

#include <map>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils.h"

namespace bl::palette {

    enum tag_type {
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

    std::string tag_type_to_str(tag_type type);

    struct abstract_tag {
       public:
        abstract_tag(const abstract_tag &) = delete;
        abstract_tag &operator=(abstract_tag &) = delete;
        explicit abstract_tag(std::string key) : key_(std::move(key)) {}

        [[nodiscard]] virtual tag_type type() const = 0;

        [[nodiscard]] virtual std::string value_string() const = 0;

        [[nodiscard]] virtual std::string to_raw() const { return ""; }

        [[nodiscard]] std::string key_to_raw() const {
            std::string res(2, '0');
            auto size = static_cast<uint16_t>(this->key_.size());
            memcpy(res.data(), &size, 2);
            return res + this->key_;
        }

        [[nodiscard]] std::string type_to_raw() const {
            return {1, static_cast<char>(this->type())};
        }

        virtual abstract_tag *copy() = 0;

        virtual void write(std::ostream &o, int indent) {
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << tag_type_to_str(this->type()) << "('" << this->key_ << "'): ";
        }

        [[nodiscard]] std::string key() const { return this->key_; }

        virtual ~abstract_tag() = default;

       protected:
        std::string key_;
    };

    struct compound_tag : public abstract_tag {
        explicit compound_tag(const std::string &key) : abstract_tag(key) {}
        compound_tag() : compound_tag("") {}
        [[nodiscard]] tag_type type() const override { return Compound; }

        void write(std::ostream &o, int indent) override {
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
        ~compound_tag() override;

        void put(const std::string &key, abstract_tag *tag) {
            auto it = this->value.find(key);
            if (it != this->value.end()) {
                delete it->second;
            }
            this->value[key] = tag;
        }

        void remove(const std::string &key) {
            auto it = this->value.find(key);
            if (it != this->value.end()) {
                delete it->second;
            }
            this->value.erase(key);
        }

        abstract_tag *copy() override {
            auto *res = new compound_tag(this->key_);
            for (auto &kv : this->value) {
                res->put(kv.first, kv.second->copy());
            }
            return res;
        }

        [[nodiscard]] std::string to_raw() const override {
            auto res = this->type_to_raw() + this->key_to_raw();
            for (auto &kv : this->value) {
                res += kv.second->to_raw();
            }
            res += std::string(1, static_cast<char>(bl::palette::tag_type::End));
            return res;
        }

        std::map<std::string, abstract_tag *> value;
    };

    struct string_tag : public abstract_tag {
        string_tag() = delete;

        explicit string_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return String; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << "'" << this->value << "'" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override { return this->value; };

        abstract_tag *copy() override {
            auto *res = new string_tag(this->key_);
            res->value = this->value;
            return res;
        }
        [[nodiscard]] std::string to_raw() const override {
            std::string res(this->value.size() + 2, '0');
            auto len = static_cast<uint16_t>(this->value.size());
            memcpy(res.data(), &len, 2);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~string_tag() override = default;
        std::string value;
    };

    struct int_tag : public abstract_tag {
        int_tag() = delete;

        explicit int_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Int; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        };
        abstract_tag *copy() override {
            auto *res = new int_tag(this->key_);
            res->value = this->value;
            return res;
        }

        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 4;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~int_tag() override = default;
        int32_t value{};
    };

    struct short_tag : public abstract_tag {
        short_tag() = delete;

        explicit short_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Short; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        ~short_tag() override = default;

        abstract_tag *copy() override {
            auto *res = new short_tag(this->key_);
            res->value = this->value;
            return res;
        }

        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 2;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        int16_t value{};
    };

    struct long_tag : public abstract_tag {
        long_tag() = delete;

        explicit long_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Long; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        };
        abstract_tag *copy() override {
            auto *res = new long_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~long_tag() override = default;

        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 8;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        int64_t value{};
    };

    struct float_tag : public abstract_tag {
        float_tag() = delete;

        explicit float_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Float; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        abstract_tag *copy() override {
            auto *res = new float_tag(this->key_);
            res->value = this->value;
            return res;
        }
        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 4;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~float_tag() override = default;
        float value{};
    };

    struct double_tag : public abstract_tag {
        double_tag() = delete;

        explicit double_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Double; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }

        abstract_tag *copy() override {
            auto *res = new double_tag(this->key_);
            res->value = this->value;
            return res;
        }

        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 8;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~double_tag() override = default;

        double value{};
    };

    struct byte_tag : public abstract_tag {
        byte_tag() = delete;

        explicit byte_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Byte; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << static_cast<int>(this->value) << std::endl;
        }
        [[nodiscard]] std::string value_string() const override {
            return std::to_string(this->value);
        }
        abstract_tag *copy() override {
            auto *res = new byte_tag(this->key_);
            res->value = this->value;
            return res;
        }
        [[nodiscard]] std::string to_raw() const override {
            const size_t N = 1;
            std::string res(N, '\0');
            memcpy(res.data(), &this->value, N);
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~byte_tag() override = default;
        uint8_t value{};
    };

    struct list_tag : public abstract_tag {
        list_tag() = delete;

        explicit list_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return List; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << "[" << this->size << "] ";
            o << "{\n";
            for (auto &tag : this->value) {
                tag->write(o, indent + 4);
            }
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << "}\n";
        }
        abstract_tag *copy() override {
            auto *res = new list_tag(this->key_);
            for (auto &item : this->value) {
                res->value.push_back(item->copy());
            }
            return res;
        }

        [[nodiscard]] std::string value_string() const override { return "[...]"; };
        void append(abstract_tag *tag) {
            if (!tag) {
                this->value.push_back(tag);
            }
        }

        [[nodiscard]] std::string to_raw() const override {
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
                res += child->to_raw();
            }
            return this->type_to_raw() + this->key_to_raw() + res;
        }

        ~list_tag() override;
        std::vector<abstract_tag *> value;
        int32_t size{0};
    };

    [[maybe_unused]] compound_tag *read_one_palette(const byte_t *data, int &read);

    std::vector<compound_tag *> read_palette_to_end(const byte_t *data, size_t len);
}  // namespace bl::palette

#endif  // BEDROCK_LEVEL_PALETTE_H
