#include <climits>
#include <exception>
#include <memory>
#include <string>

#include "binary_io.h"
#include "nbt.h"

namespace {
std::string result;
const char* last_error = "";
}  // namespace

extern "C" {

const char* nbt_to_string(const byte_t* data, size_t size) {
    result.clear();
    last_error = "";
    if (data == nullptr || size == 0 || size > INT_MAX) {
        last_error = "The NBT file is empty or too large.";
        return nullptr;
    }

    try {
        // Bedrock level.dat prefixes its little-endian NBT with two uint32 values.
        if (size >= 12 && bl::binary::read_u32_le(data + 4) == size - 8 &&
            data[8] == static_cast<byte_t>(bl::nbt::Compound)) {
            data += 8;
            size -= 8;
        }

        size_t offset = 0;
        while (offset < size) {
            auto parsed = bl::nbt::parse_one(std::span<const byte_t>(data + offset, size - offset));
            std::unique_ptr<bl::nbt::compound_tag> root(parsed.root);
            if (!parsed || !root || parsed.consumed_length == 0 || parsed.consumed_length > size - offset) {
                result.clear();
                last_error = bl::nbt::parse_error_to_string(parsed.error_code);
                return nullptr;
            }
            result += root->to_readable_string();
            offset += parsed.consumed_length;
        }
        return result.c_str();
    } catch (const std::exception&) {
        result.clear();
        last_error = "Could not convert the NBT file to text.";
        return nullptr;
    }
}

const char* nbt_last_error() { return last_error; }
size_t nbt_result_size() { return result.size(); }

}  // extern "C"
