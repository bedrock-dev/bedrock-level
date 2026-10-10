#ifndef BEDROCK_LEVEL_NBT_H
#define BEDROCK_LEVEL_NBT_H

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "binary_io.h"
#include "utils.h"

namespace bl::nbt {

    // Bedrock NBT stores all multi-byte values in little-endian order.
    namespace detail {
        using bl::binary::read_u16_le;
        using bl::binary::read_u32_le;
        using bl::binary::read_u64_le;

        template <typename T>
        T read_scalar_le(const byte_t* data) noexcept {
            static_assert(std::is_arithmetic_v<T>);
            if constexpr (std::is_same_v<T, float>) {
                return std::bit_cast<float>(read_u32_le(data));
            } else if constexpr (std::is_same_v<T, double>) {
                return std::bit_cast<double>(read_u64_le(data));
            } else if constexpr (sizeof(T) == 1) {
                return std::bit_cast<T>(static_cast<uint8_t>(data[0]));
            } else if constexpr (sizeof(T) == 2) {
                return std::bit_cast<T>(read_u16_le(data));
            } else if constexpr (sizeof(T) == 4) {
                return std::bit_cast<T>(read_u32_le(data));
            } else {
                static_assert(sizeof(T) == 8, "Unsupported scalar size");
                return std::bit_cast<T>(read_u64_le(data));
            }
        }

        using bl::binary::append_u16_le;
        using bl::binary::append_u32_le;
        using bl::binary::append_u64_le;

        template <typename T>
        void append_scalar_le(std::string& out, T value) {
            static_assert(std::is_arithmetic_v<T>);
            if constexpr (std::is_same_v<T, float>) {
                append_u32_le(out, std::bit_cast<uint32_t>(value));
            } else if constexpr (std::is_same_v<T, double>) {
                append_u64_le(out, std::bit_cast<uint64_t>(value));
            } else if constexpr (sizeof(T) == 1) {
                out.push_back(static_cast<char>(std::bit_cast<uint8_t>(value)));
            } else if constexpr (sizeof(T) == 2) {
                append_u16_le(out, std::bit_cast<uint16_t>(value));
            } else if constexpr (sizeof(T) == 4) {
                append_u32_le(out, std::bit_cast<uint32_t>(value));
            } else {
                static_assert(sizeof(T) == 8, "Unsupported scalar size");
                append_u64_le(out, std::bit_cast<uint64_t>(value));
            }
        }
    }  // namespace detail

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
        explicit abstract_tag(std::string_view key) : key_(key) {}

        abstract_tag(const abstract_tag& tag) = default;

        [[nodiscard]] std::string to_readable_string() const {
            std::stringstream s;
            this->write(s, 0);
            return s.str();
        }

        abstract_tag& operator=(const abstract_tag& tag) = default;

       public:
        [[nodiscard]] virtual tag_type type() const = 0;
        [[nodiscard]] virtual std::string value_string() const = 0;
        [[nodiscard]] virtual abstract_tag* copy() const = 0;
        [[nodiscard]] virtual std::string restricted_value_string() const { return this->value_string(); }
        [[nodiscard]] std::string to_raw() const {
            std::string out;
            out.reserve(3 + this->key_.size());
            this->write_raw(out);
            return out;
        }
        [[nodiscard]] const std::string& key() const { return this->key_; }
        void set_key(std::string_view key) { this->key_ = key; }

        void write_raw(std::string& out) const {
            out.push_back(static_cast<char>(this->type()));
            this->write_key(out);
            this->write_payload(out);
        }

        virtual void write_payload(std::string& out) const = 0;

        template <typename T>
        T as() {
            return dynamic_cast<T>(this);
        }

        template <typename T>
        T as() const {
            // Preserve the historical API: callers may request a mutable
            // pointer even when the tag itself is reached through a const
            // reference.  The non-const overload remains type-safe; this
            // compatibility overload keeps existing read/edit call sites
            // source-compatible.
            return dynamic_cast<T>(const_cast<abstract_tag*>(this));
        }

        abstract_tag* get_by_path(std::string_view path);

       public:
        virtual void write(std::ostream& o, int indent) const {
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << tag_type_to_str(this->type()) << "('" << this->key_ << "'): ";
        }

       public:
        virtual ~abstract_tag() = default;

       protected:
        void write_key(std::string& out) const {
            if (this->key_.size() > std::numeric_limits<uint16_t>::max()) {
                throw std::length_error("NBT key exceeds 16-bit length");
            }
            auto size = static_cast<uint16_t>(this->key_.size());
            detail::append_u16_le(out, size);
            out += this->key_;
        }

        std::string key_;
    };

    class tag_map {
       public:
        using value_type = std::pair<std::string, abstract_tag*>;
        using iterator = std::vector<value_type>::iterator;
        using const_iterator = std::vector<value_type>::const_iterator;

        tag_map() = default;
        tag_map(const tag_map&) = delete;
        tag_map& operator=(const tag_map&) = delete;
        tag_map(tag_map&& other) noexcept { vec_.swap(other.vec_); }
        tag_map& operator=(tag_map&& other) noexcept {
            if (this != &other) {
                clear();
                vec_.swap(other.vec_);
            }
            return *this;
        }
        ~tag_map() { clear(); }

        [[nodiscard]] iterator begin() { return vec_.begin(); }
        [[nodiscard]] iterator end() { return vec_.end(); }
        [[nodiscard]] const_iterator begin() const { return vec_.begin(); }
        [[nodiscard]] const_iterator end() const { return vec_.end(); }
        [[nodiscard]] size_t size() const { return vec_.size(); }
        [[nodiscard]] bool empty() const { return vec_.empty(); }

        [[nodiscard]] iterator find(std::string_view key) {
            auto it = lower_bound(key);
            return (it != vec_.end() && it->first == key) ? it : vec_.end();
        }

        [[nodiscard]] const_iterator find(std::string_view key) const {
            auto it = lower_bound(key);
            return (it != vec_.end() && it->first == key) ? it : vec_.end();
        }

        [[nodiscard]] size_t count(std::string_view key) const { return find(key) == end() ? 0 : 1; }

        void assign(abstract_tag* tag) {
            auto it = lower_bound(tag->key());
            if (it != vec_.end() && it->first == tag->key()) {
                delete it->second;
                it->second = tag;
            } else {
                vec_.emplace(it, tag->key(), tag);
            }
        }

        size_t erase(std::string_view key) {
            auto it = find(key);
            if (it == vec_.end()) return 0;
            delete it->second;
            vec_.erase(it);
            return 1;
        }

        void clear() {
            for (auto& [key, tag] : vec_) delete tag;
            vec_.clear();
        }

       private:
        [[nodiscard]] iterator lower_bound(std::string_view key) {
            return std::lower_bound(vec_.begin(), vec_.end(), key, [](const value_type& a, std::string_view k) { return a.first < k; });
        }

        [[nodiscard]] const_iterator lower_bound(std::string_view key) const {
            return std::lower_bound(vec_.begin(), vec_.end(), key, [](const value_type& a, std::string_view k) { return a.first < k; });
        }

        std::vector<value_type> vec_;
    };

    struct compound_tag : public abstract_tag {
        explicit compound_tag(std::string_view key) : abstract_tag(key) {}
        compound_tag(const compound_tag& tag) : abstract_tag(tag.key_) {
            this->key_ = tag.key_;
            for (auto& kv : tag.value) {
                this->value.assign(kv.second->copy());
            }
        }

        compound_tag& operator=(const compound_tag& tag) {
            if (this == &tag) return *this;
            this->value.clear();
            this->key_ = tag.key_;
            for (auto& kv : tag.value) {
                this->value.assign(kv.second->copy());
            }
            return *this;
        }

        [[nodiscard]] tag_type type() const override { return Compound; }

        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "{\n";
            for (auto& kv : this->value) {
                kv.second->write(o, indent + 4);
            }

            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << "}\n";
        }

        [[nodiscard]] std::string value_string() const override { return "(...)"; };

        void put(abstract_tag* tag) { this->value.assign(tag); }

        void remove(std::string_view key) {
            this->value.erase(key);
        }

        [[nodiscard]] abstract_tag* get(std::string_view key) {
            auto it = this->value.find(key);
            return it == this->value.end() ? nullptr : it->second;
        }

        [[nodiscard]] const abstract_tag* get(std::string_view key) const {
            auto it = this->value.find(key);
            return it == this->value.end() ? nullptr : it->second;
        }

        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new compound_tag(this->key_);
            for (auto& kv : this->value) {
                res->put(kv.second->copy());
            }
            return res;
        }

        ~compound_tag() override = default;

       public:
        void write_payload(std::string& out) const override {
            for (auto& kv : this->value) {
                kv.second->write_raw(out);
            }
            out.push_back(static_cast<char>(bl::nbt::tag_type::End));
        }

        tag_map value;
    };

    struct list_tag : public abstract_tag {
        friend class abstract_tag;

        list_tag(const list_tag& tag) : abstract_tag(tag.key_) {
            for (auto& k : tag.value) {
                this->value.push_back(k->copy());
            }
        }
        list_tag& operator=(const list_tag& tag) {
            if (this == &tag) return *this;
            for (auto* item : this->value) delete item;
            this->value.clear();
            this->key_ = tag.key_;
            for (auto& k : tag.value) {
                this->value.push_back(k->copy());
            }
            return *this;
        }
        explicit list_tag(std::string_view key) : abstract_tag(key) {}

        [[nodiscard]] tag_type type() const override { return List; }

        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[" << this->value.size() << "] ";
            o << "{\n";
            for (auto& tag : this->value) {
                tag->write(o, indent + 4);
            }
            if (indent != 0) {
                o << std::string(indent, ' ');
            }
            o << "}\n";
        }
        [[nodiscard]] std::string value_string() const override { return "[...]"; };
        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new list_tag(this->key_);
            for (auto& item : this->value) {
                res->value.push_back(item->copy());
            }

            return res;
        }
        void append(abstract_tag* tag) {
            if (tag) {
                this->value.push_back(tag);
            }
        }

        bool push_back(abstract_tag* tag) {
            if (!tag) return false;
            if (this->value.empty() || this->value[0]->type() == tag->type()) {
                this->value.push_back(tag);
                return true;
            }
            return false;
        }

        bool insert(abstract_tag* tag, size_t idx) {
            if (!tag || idx > this->value.size()) return false;
            if (this->value.empty() || this->value[0]->type() == tag->type()) {
                this->value.insert(this->value.begin() + idx, tag);
                return true;
            }
            return false;
        }

        bool remove(size_t idx) {
            if (idx >= this->value.size()) return false;
            delete this->value[idx];
            this->value.erase(this->value.begin() + idx);
            return true;
        }

        ~list_tag() override;
        std::vector<abstract_tag*> value;

       public:
        void write_payload(std::string& out) const override {
            auto child_type = End;
            if (!value.empty()) {
                child_type = value[0]->type();
            }
            out.push_back(static_cast<char>(child_type));
            if (value.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
                throw std::length_error("NBT list exceeds 32-bit length");
            }
            auto sz = static_cast<int32_t>(value.size());
            detail::append_scalar_le(out, sz);
            for (auto* child : value) {
                child->write_payload(out);
            }
        }
    };

    struct string_tag : public abstract_tag {
        explicit string_tag(std::string_view key) : abstract_tag(key) {}

        string_tag(std::string_view key, std::string value) : abstract_tag(key), value(std::move(value)) {}

        [[nodiscard]] tag_type type() const override { return String; }

        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "'" << this->value << "'" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override { return this->value; };

        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new string_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~string_tag() override = default;
        std::string value;

       public:
        void write_payload(std::string& out) const override {
            if (this->value.size() > std::numeric_limits<uint16_t>::max()) {
                throw std::length_error("NBT string exceeds 16-bit length");
            }
            auto len = static_cast<uint16_t>(this->value.size());
            detail::append_u16_le(out, len);
            out += this->value;
        }
    };

    template <typename ValueType, tag_type TT, size_t ValueSize>
    struct scalar_tag : public abstract_tag {
        explicit scalar_tag(std::string_view key) : abstract_tag(key) {}
        scalar_tag(std::string_view key, ValueType v) : abstract_tag(key), value(v) {}

        [[nodiscard]] tag_type type() const override { return TT; }

        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << this->value << std::endl;
        }
        [[nodiscard]] std::string value_string() const override { return std::to_string(this->value); }
        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new scalar_tag(this->key_);
            res->value = this->value;
            return res;
        }

        ~scalar_tag() override = default;
        ValueType value{};

       public:
        void write_payload(std::string& out) const override { detail::append_scalar_le(out, this->value); }
    };

    using short_tag = scalar_tag<int16_t, Short, 2>;
    using int_tag = scalar_tag<int32_t, Int, 4>;
    using long_tag = scalar_tag<int64_t, Long, 8>;
    using float_tag = scalar_tag<float, Float, 4>;
    using double_tag = scalar_tag<double, Double, 8>;

    // byte_tag kept standalone for static_cast<int> in write()
    struct byte_tag : public scalar_tag<int8_t, Byte, 1> {
        using scalar_tag::scalar_tag;
        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << static_cast<int>(this->value) << std::endl;
        }
        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new byte_tag(this->key_);
            res->value = this->value;
            return res;
        }
    };

    template <typename ElemType, tag_type TT>
    struct array_tag : public abstract_tag {
        explicit array_tag(std::string_view key) : abstract_tag(key) {}
        array_tag(std::string_view key, std::vector<ElemType> v) : abstract_tag(key), value(std::move(v)) {}

        [[nodiscard]] tag_type type() const override { return TT; }

        void write(std::ostream& o, int indent) const override {
            abstract_tag::write(o, indent);
            o << "[ ..." << this->value.size() << " X " << sizeof(ElemType) << " bytes ... ]" << std::endl;
        }
        [[nodiscard]] std::string value_string() const override { return "[ ..." + std::to_string(this->value.size()) + "... ]"; }
        [[nodiscard]] std::string restricted_value_string() const override { return bl::utils::numberVecToString(this->value); }

        [[nodiscard]] abstract_tag* copy() const override {
            auto* res = new array_tag(this->key_);
            res->value = this->value;
            return res;
        }
        ~array_tag() override = default;
        std::vector<ElemType> value;

       public:
        void write_payload(std::string& out) const override {
            if (this->value.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
                throw std::length_error("NBT array exceeds 32-bit length");
            }
            auto size = static_cast<int32_t>(this->value.size());
            detail::append_scalar_le(out, size);
            if constexpr (sizeof(ElemType) == 1) {
                if (!this->value.empty()) out.append(reinterpret_cast<const char*>(this->value.data()), this->value.size());
            } else {
                for (const auto value : this->value) detail::append_scalar_le(out, value);
            }
        }
    };

    using byte_array_tag = array_tag<int8_t, ByteArray>;
    using int_array_tag = array_tag<int32_t, IntArray>;
    using long_array_tag = array_tag<int64_t, LongArray>;

    compound_tag* read_one_palette(const byte_t* data, int& read);
    compound_tag* read_one_palette(const byte_t* data, size_t data_len, int& read);

    std::vector<compound_tag*> read_palette_to_end(const byte_t* data, size_t len);
}  // namespace bl::nbt

#endif  // BEDROCK_LEVEL_NBT_H
