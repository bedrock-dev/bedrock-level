//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\worlds\231)";
    auto level = bl::bedrock_level();
    level.open(path);
    if (!level.is_open()) {
        BL_LOGGER("Can not open %s", path.c_str());
    }
    level.load_global_data();
    return 0;
}