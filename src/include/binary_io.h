#ifndef BEDROCK_LEVEL_BINARY_IO_H
#define BEDROCK_LEVEL_BINARY_IO_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "utils.h"

// Bedrock's binary structures use little-endian byte order.  Keep the
// primitive operations here so callers do not depend on host alignment or
// host byte order when reading a key or a serialized payload.
namespace bl::binary {

    [[nodiscard]] inline uint16_t read_u16_le(const byte_t* data) noexcept {
        return static_cast<uint16_t>(static_cast<uint8_t>(data[0])) | (static_cast<uint16_t>(static_cast<uint8_t>(data[1])) << 8u);
    }

    [[nodiscard]] inline int16_t read_i16_le(const byte_t* data) noexcept { return static_cast<int16_t>(read_u16_le(data)); }

    [[nodiscard]] inline uint32_t read_u32_le(const byte_t* data) noexcept {
        return static_cast<uint32_t>(static_cast<uint8_t>(data[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 8u) |
               (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 16u) |
               (static_cast<uint32_t>(static_cast<uint8_t>(data[3])) << 24u);
    }

    [[nodiscard]] inline int32_t read_i32_le(const byte_t* data) noexcept { return static_cast<int32_t>(read_u32_le(data)); }

    [[nodiscard]] inline uint64_t read_u64_le(const byte_t* data) noexcept {
        uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(static_cast<uint8_t>(data[i])) << (i * 8u);
        }
        return value;
    }

    [[nodiscard]] inline int64_t read_i64_le(const byte_t* data) noexcept { return static_cast<int64_t>(read_u64_le(data)); }

    template <typename Output>
    inline void append_u16_le(Output& out, uint16_t value) {
        out.push_back(static_cast<byte_t>(value & 0xffu));
        out.push_back(static_cast<byte_t>((value >> 8u) & 0xffu));
    }

    template <typename Output>
    inline void append_i16_le(Output& out, int16_t value) {
        append_u16_le(out, static_cast<uint16_t>(value));
    }

    template <typename Output>
    inline void append_u32_le(Output& out, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) {
            out.push_back(static_cast<byte_t>((value >> (i * 8u)) & 0xffu));
        }
    }

    template <typename Output>
    inline void append_i32_le(Output& out, int32_t value) {
        append_u32_le(out, static_cast<uint32_t>(value));
    }

    template <typename Output>
    inline void append_u64_le(Output& out, uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) {
            out.push_back(static_cast<byte_t>((value >> (i * 8u)) & 0xffu));
        }
    }

    template <typename Output>
    inline void append_i64_le(Output& out, int64_t value) {
        append_u64_le(out, static_cast<uint64_t>(value));
    }

    // Cursor-based reads are used for variable-length serialized records. They
    // retain the same inline primitive operations while making truncation a
    // normal parse failure instead of an out-of-bounds access.
    class reader {
       public:
        reader(const byte_t* data, size_t size) : current_(data), end_(data + size) {}

        [[nodiscard]] bool read_u8(uint8_t& value) noexcept {
            if (!can_read(sizeof(uint8_t))) return false;
            value = static_cast<uint8_t>(current_[0]);
            current_ += sizeof(uint8_t);
            return true;
        }

        [[nodiscard]] bool read_i32_le(int32_t& value) noexcept {
            if (!can_read(sizeof(int32_t))) return false;
            value = binary::read_i32_le(current_);
            current_ += sizeof(int32_t);
            return true;
        }

        [[nodiscard]] bool read_u64_le(uint64_t& value) noexcept {
            if (!can_read(sizeof(uint64_t))) return false;
            value = binary::read_u64_le(current_);
            current_ += sizeof(uint64_t);
            return true;
        }

        [[nodiscard]] bool read_bytes(std::string& value) {
            int32_t length = 0;
            if (!read_i32_le(length) || length < 0 || !can_read(static_cast<size_t>(length))) return false;
            value.assign(current_, static_cast<size_t>(length));
            current_ += length;
            return true;
        }

        [[nodiscard]] bool skip(size_t length) noexcept {
            if (!can_read(length)) return false;
            current_ += length;
            return true;
        }

        [[nodiscard]] size_t remaining() const noexcept { return static_cast<size_t>(end_ - current_); }

       private:
        [[nodiscard]] bool can_read(size_t length) const noexcept { return length <= remaining(); }

        const byte_t* current_;
        const byte_t* end_;
    };

}  // namespace bl::binary

#endif  // BEDROCK_LEVEL_BINARY_IO_H
