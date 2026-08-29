//
// Created by xhy on 2023/3/29.
//

#include "utils.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

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
            LOG_F(ERROR, "Can not open file %s", file_name.c_str());
            return {};
        }
        std::vector<byte_t> bytes((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));
        input.close();
        return bytes;
    }

    void write_file(const std::string &file_name, const byte_t *data, size_t len) {
        std::ofstream output(file_name, std::ios::binary);
        if (!output.is_open()) {
            LOG_F(ERROR, "Can not open file %s", file_name.c_str());
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

    std::vector<std::string> splitStr(const std::string &str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void printReadableBytes(const std::string &bytes) {
        for (const auto &c : bytes) {
            if (std::isprint(c)) {
                printf("%c", c);
            }
        }
    }

    void printByteArray(const std::string &bytes) {
        for (const auto &c : bytes) {
            printf("%02X ", static_cast<unsigned char>(c));
        }
        printf("\n");
    }

    std::string toHexStr(const std::string &bytes, int n) {
        static const char hex_chars[] = "0123456789ABCDEF";
        std::string result;
        size_t len = bytes.size();
        if (len == 0) return result;
        result.reserve(len * 3);
        int group_in_line = 0;  // which 8-byte group within current line (0..n-1)
        int byte_in_group = 0;  // which byte within current 8-byte group (0..7)
        for (size_t i = 0; i < len; i++) {
            auto uc = static_cast<unsigned char>(bytes[i]);
            result.push_back(hex_chars[uc >> 4]);
            result.push_back(hex_chars[uc & 0x0F]);
            byte_in_group++;
            if (byte_in_group == 8) {
                byte_in_group = 0;
                group_in_line++;
                if (group_in_line == n) {
                    group_in_line = 0;
                    if (i + 1 < len) result += "\n";
                } else {
                    if (i + 1 < len) result += "  ";
                }
            } else {
                if (i + 1 < len) result += ' ';
            }
        }
        return result;
    }
}  // namespace bl::utils
