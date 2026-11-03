#include "nbt.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

#include "magic-enum/magic_enum.hpp"
#include "utils.h"

namespace bl::nbt {

    namespace {
        void set_error(parse_error& error, parse_error value) noexcept {
            if (error == parse_error::none) error = value;
        }
    }  // namespace

    std::tuple<abstract_tag*, size_t> read_nbt(std::span<const byte_t> data, parse_error& error);
    std::tuple<compound_tag*, size_t> read_compound_value(std::span<const byte_t> data, std::string_view key,
                                                         parse_error& error);
    std::tuple<abstract_tag*, size_t> read_value_by_type(tag_type type, std::span<const byte_t> data,
                                                        std::string_view key, parse_error& error);

    const char* parse_error_to_string(parse_error error) noexcept {
        switch (error) {
            case parse_error::none:
                return "none";
            case parse_error::empty_input:
                return "empty input";
            case parse_error::truncated_data:
                return "truncated data";
            case parse_error::invalid_tag_type:
                return "invalid tag type";
            case parse_error::invalid_root_type:
                return "root value is not a compound";
            case parse_error::invalid_array_length:
                return "invalid array length";
            case parse_error::invalid_list_length:
                return "invalid list length";
            case parse_error::missing_compound_end:
                return "missing compound end marker";
            case parse_error::invalid_consumed_length:
                return "invalid consumed length";
            case parse_error::resource_exhausted:
                return "resource exhausted";
        }
        return "unknown parse error";
    }

    std::string tag_type_to_str(tag_type type) {
        auto name = magic_enum::enum_name(type);
        return name.empty() ? "UNKNOWN" : std::string(name);
    }

    int read_string_view(std::span<const byte_t> data, std::string_view& value, parse_error& error) {
        if (data.size() < 2) {
            set_error(error, parse_error::truncated_data);
            return 0;
        }
        const uint16_t len = detail::read_u16_le(data.data());
        if (static_cast<size_t>(len) > data.size() - 2) {
            set_error(error, parse_error::truncated_data);
            return 0;
        }
        value = std::string_view(reinterpret_cast<const char*>(data.data() + 2), len);
        return len + 2;
    }

    int read_tag_type(std::span<const byte_t> data, tag_type& type, parse_error& error) {
        if (data.empty()) {
            set_error(error, parse_error::truncated_data);
            return 0;
        }
        type = static_cast<tag_type>(data[0]);
        return 1;
    }

    template <typename TagType>
    std::tuple<TagType*, size_t> read_scalar_value(std::span<const byte_t> data, std::string_view key,
                                                   parse_error& error) {
        using value_type = decltype(TagType::value);
        constexpr size_t value_size = sizeof(value_type);
        if (data.size() < value_size) {
            set_error(error, parse_error::truncated_data);
            return {nullptr, 0};
        }
        auto* tag = new TagType(key);
        tag->value = detail::read_scalar_le<value_type>(data.data());
        return {tag, value_size};
    }

    template <typename TagType>
    std::tuple<TagType*, size_t> read_array_value(std::span<const byte_t> data, std::string_view key,
                                                  parse_error& error) {
        using elem_type = typename decltype(TagType::value)::value_type;
        if (data.size() < 4) {
            set_error(error, parse_error::truncated_data);
            return {nullptr, 0};
        }
        const int32_t len = detail::read_scalar_le<int32_t>(data.data());
        if (len < 0) {
            set_error(error, parse_error::invalid_array_length);
            return {nullptr, 0};
        }
        const auto element_count = static_cast<size_t>(len);
        if (element_count > (data.size() - 4) / sizeof(elem_type)) {
            set_error(error, parse_error::invalid_array_length);
            return {nullptr, 0};
        }
        auto* tag = new TagType(key);
        tag->value = std::vector<elem_type>(element_count, 0);
        if constexpr (sizeof(elem_type) == 1) {
            if (element_count != 0) memcpy(tag->value.data(), data.data() + 4, element_count);
        } else {
            for (size_t i = 0; i < element_count; ++i) {
                tag->value[i] = detail::read_scalar_le<elem_type>(data.data() + 4 + i * sizeof(elem_type));
            }
        }
        const auto consumed = element_count * sizeof(elem_type) + 4;
        return {tag, consumed};
    }

    std::tuple<string_tag*, size_t> read_string_value(std::span<const byte_t> data, std::string_view key,
                                                      parse_error& error) {
        auto* tag = new string_tag(key);
        std::string_view value;
        const int r = read_string_view(data, value, error);
        if (r == 0) {
            delete tag;
            return {nullptr, 0};
        }
        tag->value.assign(value);
        return {tag, static_cast<size_t>(r)};
    }

    std::tuple<list_tag*, size_t> read_list_tag_value(std::span<const byte_t> data, std::string_view key,
                                                      parse_error& error) {
        size_t read = 0;
        auto* tag = new list_tag(key);
        tag_type child_type;
        {
            int r = read_tag_type(data.subspan(read), child_type, error);
            if (r == 0) {
                delete tag;
                return {nullptr, 0};
            }
            read += r;
        }
        if (data.size() - read < 4) {
            set_error(error, parse_error::truncated_data);
            delete tag;
            return {nullptr, 0};
        }
        const int32_t list_size = detail::read_scalar_le<int32_t>(data.data() + read);
        read += 4;
        if (list_size < 0 || (list_size != 0 && child_type == End)) {
            set_error(error, parse_error::invalid_list_length);
            delete tag;
            return {nullptr, 0};
        }
        const auto count = static_cast<size_t>(list_size);
        if (count > data.size() - read) {
            set_error(error, parse_error::invalid_list_length);
            delete tag;
            return {nullptr, 0};
        }
        tag->value.reserve(count);
        for (int i = 0; i < list_size; i++) {
            if (data.size() <= read) {
                set_error(error, parse_error::invalid_list_length);
                delete tag;
                return {nullptr, 0};
            }
            auto [child, sz] = read_value_by_type(child_type, data.subspan(read), "", error);
            if (child == nullptr || sz == 0 || sz > data.size() - read) {
                if (error == parse_error::none) set_error(error, parse_error::invalid_list_length);
                delete child;
                delete tag;
                return {nullptr, 0};
            }
            read += sz;
            tag->value.push_back(child);
        }
        return {tag, read};
    }

    std::tuple<compound_tag*, size_t> read_compound_value(std::span<const byte_t> data, std::string_view key,
                                                          parse_error& error) {
        auto* tag = new compound_tag(key);
        size_t total = 0;
        while (total < data.size()) {
            auto [child, read] = read_nbt(data.subspan(total), error);
            if (error != parse_error::none) {
                delete child;
                delete tag;
                return {nullptr, 0};
            }
            if (read == 0) {
                set_error(error, parse_error::truncated_data);
                delete tag;
                return {nullptr, 0};
            }
            if (read > data.size() - total) {
                set_error(error, parse_error::invalid_consumed_length);
                delete child;
                delete tag;
                return {nullptr, 0};
            }
            total += read;
            if (child) {
                tag->value.assign(child);
            } else {
                // A compound is complete only when its explicit End marker was consumed.
                if (read == 1 && data[total - 1] == static_cast<byte_t>(End)) return {tag, total};
                set_error(error, parse_error::missing_compound_end);
                delete tag;
                return {nullptr, 0};
            }
        }
        // Running out of bytes without an End marker means truncated input.
        set_error(error, parse_error::missing_compound_end);
        delete tag;
        return {nullptr, 0};
    }

    std::tuple<abstract_tag*, size_t> read_value_by_type(tag_type type, std::span<const byte_t> data,
                                                        std::string_view key, parse_error& error) {
        switch (type) {
            case Compound:
                return read_compound_value(data, key, error);
            case Int:
                return read_scalar_value<int_tag>(data, key, error);
            case Short:
                return read_scalar_value<short_tag>(data, key, error);
            case Long:
                return read_scalar_value<long_tag>(data, key, error);
            case Float:
                return read_scalar_value<float_tag>(data, key, error);
            case Double:
                return read_scalar_value<double_tag>(data, key, error);
            case Byte:
                return read_scalar_value<byte_tag>(data, key, error);
            case String:
                return read_string_value(data, key, error);
            case ByteArray:
                return read_array_value<byte_array_tag>(data, key, error);
            case IntArray:
                return read_array_value<int_array_tag>(data, key, error);
            case LongArray:
                return read_array_value<long_array_tag>(data, key, error);
            case List:
                return read_list_tag_value(data, key, error);
            default:
                set_error(error, parse_error::invalid_tag_type);
                return {nullptr, 0};
        }
    }

    std::tuple<abstract_tag*, size_t> read_nbt(std::span<const byte_t> data, parse_error& error) {
        if (data.empty()) {
            set_error(error, parse_error::truncated_data);
            return {nullptr, 0};
        }
        int read = 0;
        tag_type type;
        {
            int r = read_tag_type(data, type, error);
            if (r == 0) return {nullptr, 0};
            if (static_cast<unsigned char>(data[0]) > static_cast<unsigned char>(LongArray)) {
                set_error(error, parse_error::invalid_tag_type);
                return {nullptr, 0};
            }
            read += r;
        }
        if (type == End) {
            return {nullptr, 1};
        }
        std::string_view key;
        {
            int r = read_string_view(data.subspan(read), key, error);
            if (r == 0) return {nullptr, 0};
            read += r;
        }
        auto [tag, len] = read_value_by_type(type, data.subspan(read), key, error);
        if (tag == nullptr) {
            if (error == parse_error::none) set_error(error, parse_error::invalid_tag_type);
            return {nullptr, 0};
        }
        if (len > data.size() - static_cast<size_t>(read)) {
            set_error(error, parse_error::invalid_consumed_length);
            delete tag;
            return {nullptr, 0};
        }
        return {tag, read + len};
    }

    parse_result parse_one(std::span<const byte_t> data) {
        parse_result result;
        if (data.empty()) {
            result.error_code = parse_error::empty_input;
            return result;
        }

        parse_error error = parse_error::none;
        try {
            auto [parsed, consumed] = read_nbt(data, error);
            if (error != parse_error::none) {
                delete parsed;
                result.error_code = error;
                return result;
            }
            if (consumed == 0 || consumed > data.size()) {
                delete parsed;
                result.error_code = parse_error::invalid_consumed_length;
                return result;
            }
            if (parsed == nullptr || parsed->type() != tag_type::Compound) {
                delete parsed;
                result.error_code = parse_error::invalid_root_type;
                return result;
            }
            result.root = parsed->as<compound_tag*>();
            if (result.root == nullptr) {
                delete parsed;
                result.error_code = parse_error::invalid_root_type;
                return result;
            }
            result.consumed_length = consumed;
            return result;
        } catch (const std::bad_alloc&) {
            result.error_code = parse_error::resource_exhausted;
            return result;
        } catch (const std::length_error&) {
            result.error_code = parse_error::resource_exhausted;
            return result;
        }
    }

    parse_all_result parse_all(std::span<const byte_t> data) {
        parse_all_result result;
        if (data.empty()) {
            result.error_code = parse_error::empty_input;
            return result;
        }

        size_t ptr = 0;
        std::vector<compound_tag*> parsed;
        try {
            while (ptr < data.size()) {
                auto item = parse_one(data.subspan(ptr));
                if (!item) {
                    delete item.root;
                    for (auto* value : parsed) delete value;
                    result.error_code = item.error_code;
                    result.consumed_length = ptr;
                    return result;
                }
                if (item.consumed_length == 0 || item.consumed_length > data.size() - ptr) {
                    delete item.root;
                    for (auto* value : parsed) delete value;
                    result.error_code = parse_error::invalid_consumed_length;
                    result.consumed_length = ptr;
                    return result;
                }
                ptr += item.consumed_length;
                parsed.push_back(item.root);
            }
        } catch (const std::bad_alloc&) {
            for (auto* value : parsed) delete value;
            result.error_code = parse_error::resource_exhausted;
            result.consumed_length = ptr;
            return result;
        } catch (const std::length_error&) {
            for (auto* value : parsed) delete value;
            result.error_code = parse_error::resource_exhausted;
            result.consumed_length = ptr;
            return result;
        }
        result.consumed_length = ptr;
        result.roots = std::move(parsed);
        return result;
    }
    list_tag::~list_tag() {
        for (auto tag : this->value) {
            delete tag;
        }
    }

    abstract_tag* abstract_tag::get_by_path(std::string_view path) {
        abstract_tag* cur = this;
        size_t i = 0;
        while (cur && i < path.size()) {
            if (path[i] == '.') {
                i++;
                continue;
            }
            if (path[i] != '[') {
                auto end = path.find_first_of(".[", i);
                auto name = path.substr(i, end == std::string_view::npos ? std::string_view::npos : end - i);
                i = (end == std::string_view::npos) ? path.size() : end;
                auto* comp = cur->as<compound_tag*>();
                if (!comp) return nullptr;
                cur = comp->get(name);
            }
            while (cur && i < path.size() && path[i] == '[') {
                auto close = path.find(']', i);
                if (close == std::string_view::npos) return nullptr;
                int idx = 0;
                bool valid = close > i + 1;
                for (auto k = i + 1; k < close && valid; k++) {
                    if (path[k] < '0' || path[k] > '9') {
                        valid = false;
                    } else {
                        idx = idx * 10 + (path[k] - '0');
                    }
                }
                auto* list = cur->as<list_tag*>();
                if (!valid || !list || idx >= static_cast<int>(list->value.size())) return nullptr;
                cur = list->value[idx];
                i = close + 1;
            }
        }
        return cur;
    }
}  // namespace bl::nbt
