//
// Created by xhy on 2023/3/29.
//

#include "utils.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

void log(const char *file_name, const char *function_name, size_t line, const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "[INFO] [%s:%zu @ %s]:", file_name, line, function_name);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);
#endif
}

void error_msg(const char *file_name, const char *function_name, size_t line, const char *fmt,
               ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "[ERROR] [%s:%zu @ %s]:", file_name, line, function_name);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);
}

void M_Assert(const char *expr_str, bool expr, const char *file, int line, const char *msg, ...) {
    if (!expr) {
        fprintf(stderr, "Assert failed:\t");
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        fprintf(stderr, "\nExpected: %s\n", expr_str);
        fprintf(stderr, "At Source: %s:%d\n", file, line);
        abort();
    }
}
#include <filesystem>
namespace bl::utils {

    std::vector<byte_t> read_file(const std::string &file_name) {
        std::ifstream input(std::filesystem::u8path(file_name), std::ios::binary);
        if (!input.is_open()) {
            BL_ERROR("Can not open file %s", file_name.c_str());
            return {};
        }
        std::vector<byte_t> bytes((std::istreambuf_iterator<char>(input)),
                                  (std::istreambuf_iterator<char>()));
        input.close();
        return bytes;
    }

    void write_file(const std::string &file_name, const byte_t *data, size_t len) {
        std::ofstream output(file_name, std::ios::binary);
        if (!output.is_open()) {
            BL_ERROR("Can not open file %s", file_name.c_str());
            return;
        }
        output.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(len));
        output.close();
    }
    //    https :  // www.jianshu.com/p/baf75216f883

#ifdef _WIN32
#include <windows.h>
    std::string UTF8ToGBEx(const char *utf8) {
        if (!utf8 || strlen(utf8) < 1) return "";
        std::stringstream ss;
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        wchar_t *wstr = new wchar_t[len + 1];
        memset(wstr, 0, len + 1);
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
        len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        char *str = new char[len + 1];
        memset(str, 0, len + 1);
        WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, nullptr, nullptr);
        ss << str;
        delete[] wstr;
        delete[] str;
        return ss.str();
    }
#else
    std::string UTF8ToGBEx(const char *utf8) { return std::string(utf8); }
#endif

}  // namespace bl::utils
