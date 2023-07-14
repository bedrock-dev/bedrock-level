//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
#include "utils.h"
void check_leak() {
    const std::string path = R"(C:\Users\xhy\Desktop\SAC_survival)";
    auto *level = new bl::bedrock_level();
    level->open(path);
    if (!level->is_open()) {
        BL_LOGGER("Can not open %s", path.c_str());
        return;
    }

    int x = 0;
    PROF_TIMER(TR, {
        level->foreach_global_keys([&x](const std::string &key, const std::string &value) { x++; });
    });

    printf("Total %d keys, %zu ms needed", x, time_TR / 1000);
}

int main() {
    check_leak();
    return 0;
}