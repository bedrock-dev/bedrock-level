//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
#include "utils.h"
void check_leak() {
    const std::string path = R"(C:\Users\xhy\Desktop\2g)";
    auto* level = new bl::bedrock_level();
    level->open(path);
    if (!level->is_open()) {
        BL_LOGGER("Can not open %s", path.c_str());
        return;
    }
    PROF_TIMER(fast, {
        for (int i = 0; i < 40; i++) {
            for (int j = 0; j < 40; j++) {
                level->get_chunk(bl::chunk_pos{i, j, 0});
            }
        }
    });

    BL_LOGGER("time is %d ms", time_fast / 1000);
}

int main() {
    check_leak();
    return 0;
}