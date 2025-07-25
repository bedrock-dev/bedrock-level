//
// Created by xhy on 2023/3/29.
//

#ifndef BEDROCK_LEVEL_UTILS_H
#define BEDROCK_LEVEL_UTILS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

typedef std::chrono::high_resolution_clock timer_clock;
typedef int64_t microsecond_t;

#define PROF_TIMER(label, Codes)                                                 \
    auto start_##label = std::chrono::high_resolution_clock::now();              \
    { Codes }                                                                    \
    auto e_##label = std::chrono::high_resolution_clock ::now() - start_##label; \
    auto time_##label = std::chrono::duration_cast<std::chrono::microseconds>(e_##label).count();

#define DEBUG

// #ifdef WIN32
// #define FN (__builtin_strrchr(__FILE__, '\\') ? __builtin_strrchr(__FILE__, '\\') + 1 : __FILE__)
// #else
// #define FN (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#define FN ""
// #endif
#define BL_LOGGER(...) log(FN, __FUNCTION__, __LINE__, __VA_ARGS__)
#define BL_ERROR(...) error_msg(FN, __FUNCTION__, __LINE__, __VA_ARGS__)

void log(const char *file_name, const char *function_name, size_t line, const char *fmt, ...);

void error_msg(const char *file_name, const char *function_name, size_t line, const char *fmt, ...);

#ifdef DEBUG
#define Assert(Expr, ...) M_Assert(#Expr, Expr, __FILE__, __LINE__, __VA_ARGS__)
#else
#define Assert(Expr, Msg) ;
#endif

typedef char byte_t;
static_assert(sizeof(byte_t) == 1);

void M_Assert(const char *expr_str, bool expr, const char *file, int line, const char *fmt, ...);

// disable data copy
struct NonCopyable {
    NonCopyable &operator=(const NonCopyable &) = delete;

    NonCopyable(const NonCopyable &) = delete;

    NonCopyable() = default;
};

namespace bl::utils {
    std::vector<byte_t> read_file(const std::string &file_name);

    void write_file(const std::string &file_name, const byte_t *data, size_t len);

    std::string UTF8ToGBEx(const char *utf8);

    template <typename T>
    std::string numberVecToString(const std::vector<T> &vec, const std::string &sep = " ") {
        std::string res;
        if (vec.empty()) return res;
        res.reserve(vec.size() * 4);
        for (auto i = 0ul; i < vec.size() - 1; i++) {
            res += std::to_string(vec[i]) + sep;
        }
        res += std::to_string(vec.back());
        return res;
    }
}  // namespace bl::utils

#endif  // BEDROCK_LEVEL_UTILS_H
