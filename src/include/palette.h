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

namespace bl::palette {

    enum tag_type { End = 0, Byte = 1, Int = 3, String = 8, Compound = 10, LEN = 11 };

    std::string tag_type_to_str(tag_type type);

    struct abstract_tag {
       public:
        explicit abstract_tag(std::string key) : key_(std::move(key)) {}

        [[nodiscard]] virtual tag_type type() const = 0;

        virtual void write(std::ostream &o, int indent) {
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << tag_type_to_str(this->type()) << "('" << this->key_ << "'): ";
        }

        [[nodiscard]] std::string key() const { return this->key_; }

       protected:
        std::string key_;
    };

    struct compound_tag : public abstract_tag {
        compound_tag() = delete;

        explicit compound_tag(const std::string &key) : abstract_tag(key) {}

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

        int32_t value{};
    };

    struct byte_tag : public abstract_tag {
        byte_tag() = delete;

        explicit byte_tag(const std::string &key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return Byte; }

        void write(std::ostream &o, int indent) override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }

        uint8_t value{};
    };

    [[maybe_unused]] compound_tag *read_one_palette(const uint8_t *data, int &read);
}  // namespace bl::palette

#endif  // BEDROCK_LEVEL_PALETTE_H
