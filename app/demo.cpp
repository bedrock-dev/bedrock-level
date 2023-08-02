//
// Created by xhy on 2023/6/22.
//
#include <iostream>
#include <list>
#include <unordered_map>
#include <utility>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "palette.h"
int main() {
    std::locale::global(std::locale(""));
    auto name = R"(D:\MC\saves\中文测试)";
    bl::bedrock_level level;
    level.open(name);
    //    level.foreach_global_keys([](const std::string& key, const std::string& value) {});
    //    level.load_global_data();
    return 0;
}
